/*
 * Copyright © 2019 FMSoft Technologies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    WEI Yongming <vincent@minigui.org>
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_LIBDRM
#include <xf86drm.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include "util/debug.h"
#include "util/macros.h"
#include "util/u_vector.h"
#include "util/anon_file.h"

#include "egl_dri2.h"
#include "egl_dri2_fallbacks.h"
#include "loader.h"

#ifdef HAVE_LIBDRM
#include <drm/drm_fourcc.h>
#else
// TODO define format without fourcc file.
#endif

/*
 * The index of entries in this table is used as a bitmask in
 * dri2_dpy->mg_formats, which tracks the formats supported by MiniGUI.
 */
static const struct dri2_minigui_visual {
   const char *format_name;
   uint32_t drm_format;
   int dri_image_format;
   /* alt_dri_image_format is a substitute mg_buffer format to use for a
    * wl-server unsupported dri_image_format, ie. some other dri_image_format in
    * the table, of the same precision but with different channel ordering, or
    * __DRI_IMAGE_FORMAT_NONE if an alternate format is not needed or supported.
    * The code checks if alt_dri_image_format can be used as a fallback for a
    * dri_image_format for a given wl-server implementation.
    */
   int alt_dri_image_format;
   int bpp;
   unsigned int rgba_masks[4];
} dri2_mg_visuals[] = {
   {
     "XRGB2101010",
     DRM_FORMAT_XRGB2101010,
     __DRI_IMAGE_FORMAT_XRGB2101010, __DRI_IMAGE_FORMAT_XBGR2101010, 32,
     { 0x3ff00000, 0x000ffc00, 0x000003ff, 0x00000000 }
   },
   {
     "ARGB2101010",
     DRM_FORMAT_ARGB2101010,
     __DRI_IMAGE_FORMAT_ARGB2101010, __DRI_IMAGE_FORMAT_ABGR2101010, 32,
     { 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 }
   },
   {
     "XBGR2101010",
     DRM_FORMAT_XBGR2101010,
     __DRI_IMAGE_FORMAT_XBGR2101010, __DRI_IMAGE_FORMAT_XRGB2101010, 32,
     { 0x000003ff, 0x000ffc00, 0x3ff00000, 0x00000000 }
   },
   {
     "ABGR2101010",
     DRM_FORMAT_ABGR2101010,
     __DRI_IMAGE_FORMAT_ABGR2101010, __DRI_IMAGE_FORMAT_ARGB2101010, 32,
     { 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000 }
   },
   {
     "XRGB8888",
     DRM_FORMAT_XRGB8888,
     __DRI_IMAGE_FORMAT_XRGB8888, __DRI_IMAGE_FORMAT_NONE, 32,
     { 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 }
   },
   {
     "ARGB8888",
     DRM_FORMAT_ARGB8888,
     __DRI_IMAGE_FORMAT_ARGB8888, __DRI_IMAGE_FORMAT_NONE, 32,
     { 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 }
   },
   {
     "RGB565",
     DRM_FORMAT_RGB565,
     __DRI_IMAGE_FORMAT_RGB565, __DRI_IMAGE_FORMAT_NONE, 16,
     { 0xf800, 0x07e0, 0x001f, 0x0000 }
   },
};

static_assert(ARRAY_SIZE(dri2_mg_visuals) <= EGL_DRI2_MAX_FORMATS,
              "dri2_egl_display::formats is not large enough for "
              "the formats in dri2_mg_visuals");

static int
dri2_minigui_visual_idx_from_config(struct dri2_egl_display *dri2_dpy,
                               const __DRIconfig *config)
{
   unsigned int red, green, blue, alpha;

   dri2_dpy->core->getConfigAttrib(config, __DRI_ATTRIB_RED_MASK, &red);
   dri2_dpy->core->getConfigAttrib(config, __DRI_ATTRIB_GREEN_MASK, &green);
   dri2_dpy->core->getConfigAttrib(config, __DRI_ATTRIB_BLUE_MASK, &blue);
   dri2_dpy->core->getConfigAttrib(config, __DRI_ATTRIB_ALPHA_MASK, &alpha);

   for (unsigned int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      const struct dri2_minigui_visual *mg_visual = &dri2_mg_visuals[i];

      if (red == mg_visual->rgba_masks[0] &&
          green == mg_visual->rgba_masks[1] &&
          blue == mg_visual->rgba_masks[2] &&
          alpha == mg_visual->rgba_masks[3]) {
         return i;
      }
   }

   return -1;
}

static int
dri2_minigui_visual_idx_from_fourcc(uint32_t fourcc)
{
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      /* mg_drm format codes overlap with DRIImage FourCC codes for all formats
       * we support. */
      if (dri2_mg_visuals[i].drm_format == fourcc)
         return i;
   }

   return -1;
}

static int
dri2_minigui_visual_idx_from_dri_image_format(uint32_t dri_image_format)
{
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (dri2_mg_visuals[i].dri_image_format == dri_image_format)
         return i;
   }

   return -1;
}

#if 0 // VW
static int
dri2_minigui_visual_idx_from_shm_format(uint32_t shm_format)
{
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (dri2_mg_visuals[i].mg_shm_format == shm_format)
         return i;
   }

   return -1;
}

static bool
dri2_minigui_is_format_supported(void* user_data, uint32_t format)
{
   _EGLDisplay *disp = (_EGLDisplay *) user_data;
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   int j = dri2_minigui_visual_idx_from_fourcc(format);

   if (j == -1)
      return false;

   for (int i = 0; dri2_dpy->driver_configs[i]; i++)
      if (j == dri2_minigui_visual_idx_from_config(dri2_dpy,
                                              dri2_dpy->driver_configs[i]))
         return true;

   return false;
}
#endif

/* Get red channel mask for given depth. */
static unsigned int
dri2_minigui_get_red_mask_for_depth(struct dri2_egl_display *dri2_dpy, int depth)
{
   /* VW: TODO */
   return 0;
}

static uint32_t
dri2_format_for_depth(struct dri2_egl_display *dri2_dpy, uint32_t depth)
{
   switch (depth) {
   case 16:
      return __DRI_IMAGE_FORMAT_RGB565;
   case 24:
      return __DRI_IMAGE_FORMAT_XRGB8888;
   case 30:
      /* Different preferred formats for different hw */
      if (dri2_minigui_get_red_mask_for_depth(dri2_dpy, 30) == 0x3ff)
         return __DRI_IMAGE_FORMAT_XBGR2101010;
      else
         return __DRI_IMAGE_FORMAT_XRGB2101010;
   case 32:
      return __DRI_IMAGE_FORMAT_ARGB8888;
   default:
      return __DRI_IMAGE_FORMAT_NONE;
   }
}


static void
swrastCreateDrawable(struct dri2_egl_display * dri2_dpy,
                     struct dri2_egl_surface * dri2_surf)
{
#if 0
   uint32_t           mask;
   const uint32_t     function = GXcopy;
   uint32_t           valgc[2];

   /* create GC's */
   dri2_surf->gc = xcb_generate_id(dri2_dpy->conn);
   mask = XCB_GC_FUNCTION;
   xcb_create_gc(dri2_dpy->conn, dri2_surf->gc, dri2_surf->drawable, mask, &function);

   dri2_surf->swapgc = xcb_generate_id(dri2_dpy->conn);
   mask = XCB_GC_FUNCTION | XCB_GC_GRAPHICS_EXPOSURES;
   valgc[0] = function;
   valgc[1] = False;
   xcb_create_gc(dri2_dpy->conn, dri2_surf->swapgc, dri2_surf->drawable, mask, valgc);

   switch (dri2_surf->depth) {
      case 32:
      case 30:
      case 24:
         dri2_surf->bytes_per_pixel = 4;
         break;
      case 16:
         dri2_surf->bytes_per_pixel = 2;
         break;
      case 8:
         dri2_surf->bytes_per_pixel = 1;
         break;
      case 0:
         dri2_surf->bytes_per_pixel = 0;
         break;
      default:
         _eglLog(_EGL_WARNING, "unsupported depth %d", dri2_surf->depth);
   }
#endif
}

static void
swrastDestroyDrawable(struct dri2_egl_display * dri2_dpy,
                      struct dri2_egl_surface * dri2_surf)
{
#if 0
   xcb_free_gc(dri2_dpy->conn, dri2_surf->gc);
   xcb_free_gc(dri2_dpy->conn, dri2_surf->swapgc);
#endif
}

static int
dri2_minigui_swrast_get_stride_for_format(int format, int w)
{
#if 0 // VW
   int visual_idx = dri2_minigui_visual_idx_from_shm_format(format);

   assume(visual_idx != -1);

   return w * (dri2_mg_visuals[visual_idx].bpp / 8);
#else
   return w * 2;
#endif
}

static inline void mg_buffer_destroy(HDC dc)
{
    DeleteMemDC(dc);
}

static EGLBoolean
dri2_minigui_swrast_allocate_buffer(struct dri2_egl_surface *dri2_surf,
                               int format, int w, int h,
                               void **data, int *size)
{
#if 0
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
#endif
   int fd, stride, size_map;
   void *data_map;

   stride = dri2_minigui_swrast_get_stride_for_format(format, w);
   size_map = h * stride;

   /* Create a shareable buffer */
   fd = os_create_anonymous_file(size_map, NULL);
   if (fd < 0)
      return EGL_FALSE;

   data_map = mmap(NULL, size_map, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (data_map == MAP_FAILED) {
      close(fd);
      return EGL_FALSE;
   }

   *data = data_map;
   *size = size_map;
   return EGL_TRUE;
}

static int
swrast_update_buffers(struct dri2_egl_surface *dri2_surf)
{
#if 0 // VW
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   /* we need to do the following operations only once per frame */
   if (dri2_surf->mg_back)
      return 0;

   if (dri2_surf->base.Width != dri2_surf->mg_win->width ||
       dri2_surf->base.Height != dri2_surf->mg_win->height) {

      dri2_minigui_release_buffers(dri2_surf);

      dri2_surf->base.Width  = dri2_surf->mg_win->width;
      dri2_surf->base.Height = dri2_surf->mg_win->height;
      dri2_surf->dx = dri2_surf->mg_win->dx;
      dri2_surf->dy = dri2_surf->mg_win->dy;
      dri2_surf->mg_current = NULL;
   }

   /* find back buffer */

   /* There might be a buffer release already queued that wasn't processed */
   mg_display_dispatch_queue_pending(dri2_dpy->mg_dpy, dri2_surf->mg_queue);

   /* try get free buffer already created */
   for (int i = 0; i < ARRAY_SIZE(dri2_surf->mg_color_buffers); i++) {
      if (!dri2_surf->mg_color_buffers[i].locked &&
          dri2_surf->mg_color_buffers[i].mg_buffer) {
          dri2_surf->mg_back = &dri2_surf->mg_color_buffers[i];
          break;
      }
   }

   /* else choose any another free location */
   if (!dri2_surf->mg_back) {
      for (int i = 0; i < ARRAY_SIZE(dri2_surf->mg_color_buffers); i++) {
         if (!dri2_surf->mg_color_buffers[i].locked) {
             dri2_surf->mg_back = &dri2_surf->mg_color_buffers[i];
             if (!dri2_minigui_swrast_allocate_buffer(dri2_surf,
                                                 dri2_surf->mg_format,
                                                 dri2_surf->base.Width,
                                                 dri2_surf->base.Height,
                                                 &dri2_surf->mg_back->data,
                                                 &dri2_surf->mg_back->data_size,
                                                 &dri2_surf->mg_back->mg_buffer)) {
                _eglError(EGL_BAD_ALLOC, "failed to allocate color buffer");
                 return -1;
             }
             mg_buffer_add_listener(dri2_surf->mg_back->mg_buffer,
                                    &mg_buffer_listener, dri2_surf);
             break;
         }
      }
   }

   if (!dri2_surf->mg_back) {
      _eglError(EGL_BAD_ALLOC, "failed to find free buffer");
      return -1;
   }

   dri2_surf->mg_back->locked = true;

   /* If we have an extra unlocked buffer at this point, we had to do triple
    * buffering for a while, but now can go back to just double buffering.
    * That means we can free any unlocked buffer now. */
   for (int i = 0; i < ARRAY_SIZE(dri2_surf->mg_color_buffers); i++) {
      if (!dri2_surf->mg_color_buffers[i].locked &&
          dri2_surf->mg_color_buffers[i].mg_buffer) {
         mg_buffer_destroy(dri2_surf->mg_color_buffers[i].mg_buffer);
         munmap(dri2_surf->mg_color_buffers[i].data,
                dri2_surf->mg_color_buffers[i].data_size);
         dri2_surf->mg_color_buffers[i].mg_buffer = NULL;
         dri2_surf->mg_color_buffers[i].data = NULL;
      }
   }

#endif
   return 0;
}

static void*
dri2_minigui_swrast_get_frontbuffer_data(struct dri2_egl_surface *dri2_surf)
{
   /* if there has been a resize: */
   if (!dri2_surf->mg_current)
      return NULL;

   return dri2_surf->mg_current->data;
}

static void*
dri2_minigui_swrast_get_backbuffer_data(struct dri2_egl_surface *dri2_surf)
{
   assert(dri2_surf->mg_back);
   return dri2_surf->mg_back->data;
}

static void
dri2_minigui_swrast_commit_backbuffer(struct dri2_egl_surface *dri2_surf)
{
#if 0
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(dri2_surf->base.Resource.Display);

   while (dri2_surf->throttle_callback != NULL)
      if (mg_display_dispatch_queue(dri2_dpy->mg_dpy,
                                    dri2_surf->mg_queue) == -1)
         return;

   if (dri2_surf->base.SwapInterval > 0) {
      dri2_surf->throttle_callback =
         mg_surface_frame(dri2_surf->mg_surface_wrapper);
      mg_callback_add_listener(dri2_surf->throttle_callback,
                               &throttle_listener, dri2_surf);
   }

   dri2_surf->mg_current = dri2_surf->mg_back;
   dri2_surf->mg_back = NULL;

   mg_surface_attach(dri2_surf->mg_surface_wrapper,
                     dri2_surf->mg_current->mg_buffer,
                     dri2_surf->dx, dri2_surf->dy);

   dri2_surf->mg_win->attached_width  = dri2_surf->base.Width;
   dri2_surf->mg_win->attached_height = dri2_surf->base.Height;
   /* reset resize growing parameters */
   dri2_surf->dx = 0;
   dri2_surf->dy = 0;

   mg_surface_damage(dri2_surf->mg_surface_wrapper,
                     0, 0, INT32_MAX, INT32_MAX);
   mg_surface_commit(dri2_surf->mg_surface_wrapper);

   /* If we're not waiting for a frame callback then we'll at least throttle
    * to a sync callback so that we always give a chance for the compositor to
    * handle the commit and send a release event before checking for a free
    * buffer */
   if (dri2_surf->throttle_callback == NULL) {
      dri2_surf->throttle_callback = mg_display_sync(dri2_surf->mg_dpy_wrapper);
      mg_callback_add_listener(dri2_surf->throttle_callback,
                               &throttle_listener, dri2_surf);
   }

   mg_display_flush(dri2_dpy->mg_dpy);
#endif
}

static void
dri2_minigui_swrast_get_drawable_info(__DRIdrawable * draw,
                                 int *x, int *y, int *w, int *h,
                                 void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;

   (void) swrast_update_buffers(dri2_surf);
   *x = 0;
   *y = 0;
   *w = dri2_surf->base.Width;
   *h = dri2_surf->base.Height;
}

static void
dri2_minigui_swrast_get_image(__DRIdrawable * read,
                         int x, int y, int w, int h,
                         char *data, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   int copy_width = dri2_minigui_swrast_get_stride_for_format(dri2_surf->mg_format, w);
   int x_offset = dri2_minigui_swrast_get_stride_for_format(dri2_surf->mg_format, x);
   int src_stride = dri2_minigui_swrast_get_stride_for_format(dri2_surf->mg_format, dri2_surf->base.Width);
   int dst_stride = copy_width;
   char *src, *dst;

   src = dri2_minigui_swrast_get_frontbuffer_data(dri2_surf);
   if (!src) {
      memset(data, 0, copy_width * h);
      return;
   }

   assert(data != src);
   assert(copy_width <= src_stride);

   src += x_offset;
   src += y * src_stride;
   dst = data;

   if (copy_width > src_stride-x_offset)
      copy_width = src_stride-x_offset;
   if (h > dri2_surf->base.Height-y)
      h = dri2_surf->base.Height-y;

   for (; h>0; h--) {
      memcpy(dst, src, copy_width);
      src += src_stride;
      dst += dst_stride;
   }
}

static void
dri2_minigui_swrast_put_image2(__DRIdrawable * draw, int op,
                         int x, int y, int w, int h, int stride,
                         char *data, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   int copy_width = dri2_minigui_swrast_get_stride_for_format(dri2_surf->mg_format, w);
   int dst_stride = dri2_minigui_swrast_get_stride_for_format(dri2_surf->mg_format, dri2_surf->base.Width);
   int x_offset = dri2_minigui_swrast_get_stride_for_format(dri2_surf->mg_format, x);
   char *src, *dst;

   assert(copy_width <= stride);

   (void) swrast_update_buffers(dri2_surf);
   dst = dri2_minigui_swrast_get_backbuffer_data(dri2_surf);

   /* partial copy, copy old content */
   if (copy_width < dst_stride)
      dri2_minigui_swrast_get_image(draw, 0, 0,
                               dri2_surf->base.Width, dri2_surf->base.Height,
                               dst, loaderPrivate);

   dst += x_offset;
   dst += y * dst_stride;

   src = data;

   /* drivers expect we do these checks (and some rely on it) */
   if (copy_width > dst_stride-x_offset)
      copy_width = dst_stride-x_offset;
   if (h > dri2_surf->base.Height-y)
      h = dri2_surf->base.Height-y;

   for (; h>0; h--) {
      memcpy(dst, src, copy_width);
      src += stride;
      dst += dst_stride;
   }
   dri2_minigui_swrast_commit_backbuffer(dri2_surf);
}

static void
dri2_minigui_swrast_put_image(__DRIdrawable * draw, int op,
                         int x, int y, int w, int h,
                         char *data, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   int stride;

   stride = dri2_minigui_swrast_get_stride_for_format(dri2_surf->mg_format, w);
   dri2_minigui_swrast_put_image2(draw, op, x, y, w, h,
                             stride, data, loaderPrivate);
}

/**
 * Called via eglCreateWindowSurface(), drv->API.CreateWindowSurface().
 */
static _EGLSurface *
dri2_minigui_create_window_surface(_EGLDriver *drv, _EGLDisplay *disp,
                        _EGLConfig *conf, void *native_window,
                        const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *config;

   (void) drv;

   dri2_surf = calloc(1, sizeof *dri2_surf);
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "dri2_create_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, EGL_WINDOW_BIT, conf, attrib_list,
                          false, native_window))
      goto cleanup_surf;

   config = dri2_get_dri_config(dri2_conf, EGL_WINDOW_BIT,
                                dri2_surf->base.GLColorspace);

   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_pixmap;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_pixmap;

   // VW: TODO

   return &dri2_surf->base;

 cleanup_dri_drawable:
   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
 cleanup_pixmap:
 cleanup_surf:
   free(dri2_surf);

   return NULL;
}

static _EGLSurface *
dri2_minigui_create_pixmap_surface(_EGLDriver *drv, _EGLDisplay *disp,
                               _EGLConfig *conf, void *native_pixmap,
                               const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *config;

   (void) drv;

   dri2_surf = calloc(1, sizeof *dri2_surf);
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "dri2_create_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, EGL_PIXMAP_BIT, conf, attrib_list,
                          false, native_pixmap))
      goto cleanup_surf;

   config = dri2_get_dri_config(dri2_conf, EGL_PIXMAP_BIT,
                                dri2_surf->base.GLColorspace);

   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_pixmap;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_pixmap;

   // VW: TODO

   return &dri2_surf->base;

 cleanup_dri_drawable:
   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
 cleanup_pixmap:
 cleanup_surf:
   free(dri2_surf);

   return NULL;
}

static EGLBoolean
dri2_minigui_destroy_surface(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   (void) drv;

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);

#if 0 // VW
   for (int i = 0; i < ARRAY_SIZE(dri2_surf->mg_color_buffers); i++) {
      if (dri2_surf->mg_color_buffers[i].mg_buffer)
         mg_buffer_destroy(dri2_surf->mg_color_buffers[i].mg_buffer);
      if (dri2_surf->mg_color_buffers[i].dri_image)
         dri2_dpy->image->destroyImage(dri2_surf->mg_color_buffers[i].dri_image);
      if (dri2_surf->mg_color_buffers[i].linear_copy)
         dri2_dpy->image->destroyImage(dri2_surf->mg_color_buffers[i].linear_copy);
      if (dri2_surf->mg_color_buffers[i].data)
         munmap(dri2_surf->mg_color_buffers[i].data,
                dri2_surf->mg_color_buffers[i].data_size);
   }
#endif

   if (dri2_dpy->dri2)
      dri2_egl_surface_free_local_buffers(dri2_surf);

#if 0 // VW
   if (dri2_surf->throttle_callback)
      mg_callback_destroy(dri2_surf->throttle_callback);

   if (dri2_surf->mg_win) {
      dri2_surf->mg_win->driver_private = NULL;
      dri2_surf->mg_win->resize_callback = NULL;
      dri2_surf->mg_win->destroy_window_callback = NULL;
   }

   mg_proxy_wrapper_destroy(dri2_surf->mg_surface_wrapper);
   mg_proxy_wrapper_destroy(dri2_surf->mg_dpy_wrapper);
   if (dri2_surf->mg_drm_wrapper)
      mg_proxy_wrapper_destroy(dri2_surf->mg_drm_wrapper);
   mg_event_queue_destroy(dri2_surf->mg_queue);
#endif

   dri2_fini_surface(surf);
   free(surf);

   return EGL_TRUE;
}

static void
dri2_minigui_release_buffers(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   for (int i = 0; i < ARRAY_SIZE(dri2_surf->mg_color_buffers); i++) {
      if (dri2_surf->mg_color_buffers[i].mg_buffer) {
         if (dri2_surf->mg_color_buffers[i].locked) {
            dri2_surf->mg_color_buffers[i].mg_release = true;
         } else {
            mg_buffer_destroy(dri2_surf->mg_color_buffers[i].mg_buffer);
            dri2_surf->mg_color_buffers[i].mg_buffer = NULL;
         }
      }
      if (dri2_surf->mg_color_buffers[i].dri_image)
         dri2_dpy->image->destroyImage(dri2_surf->mg_color_buffers[i].dri_image);
      if (dri2_surf->mg_color_buffers[i].linear_copy)
         dri2_dpy->image->destroyImage(dri2_surf->mg_color_buffers[i].linear_copy);
      if (dri2_surf->mg_color_buffers[i].data)
         munmap(dri2_surf->mg_color_buffers[i].data,
                dri2_surf->mg_color_buffers[i].data_size);

      dri2_surf->mg_color_buffers[i].dri_image = NULL;
      dri2_surf->mg_color_buffers[i].linear_copy = NULL;
      dri2_surf->mg_color_buffers[i].data = NULL;
   }

   if (dri2_dpy->dri2)
      dri2_egl_surface_free_local_buffers(dri2_surf);
}

static int
get_back_bo(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   int use_flags;
   int visual_idx;
   unsigned int dri_image_format;
   unsigned int linear_dri_image_format;
   uint64_t *modifiers;
   int num_modifiers;

   visual_idx = dri2_minigui_visual_idx_from_fourcc(dri2_surf->mg_format);
   assert(visual_idx != -1);
   dri_image_format = dri2_mg_visuals[visual_idx].dri_image_format;
   linear_dri_image_format = dri_image_format;
   modifiers = u_vector_tail(&dri2_dpy->mg_modifiers[visual_idx]);
   num_modifiers = u_vector_length(&dri2_dpy->mg_modifiers[visual_idx]);

   /* Substitute dri image format if server does not support original format */
   if (!BITSET_TEST(dri2_dpy->mg_formats, visual_idx))
      linear_dri_image_format = dri2_mg_visuals[visual_idx].alt_dri_image_format;

   /* These asserts hold, as long as dri2_mg_visuals[] is self-consistent and
    * the PRIME substitution logic in dri2_minigui_add_configs_for_visuals() is free
    * of bugs.
    */
   assert(linear_dri_image_format != __DRI_IMAGE_FORMAT_NONE);
   assert(BITSET_TEST(dri2_dpy->mg_formats,
          dri2_minigui_visual_idx_from_dri_image_format(linear_dri_image_format)));

   /* There might be a buffer release already queued that wasn't processed */
   // VW: mg_display_dispatch_queue_pending(dri2_dpy->mg_dpy, dri2_surf->mg_queue);

   while (dri2_surf->mg_back == NULL) {
      for (int i = 0; i < ARRAY_SIZE(dri2_surf->mg_color_buffers); i++) {
         /* Get an unlocked buffer, preferably one with a dri_buffer
          * already allocated. */
         if (dri2_surf->mg_color_buffers[i].locked)
            continue;
         if (dri2_surf->mg_back == NULL)
            dri2_surf->mg_back = &dri2_surf->mg_color_buffers[i];
         else if (dri2_surf->mg_back->dri_image == NULL)
            dri2_surf->mg_back = &dri2_surf->mg_color_buffers[i];
      }

      if (dri2_surf->mg_back)
         break;

      /* If we don't have a buffer, then block on the server to release one for
       * us, and try again. mg_display_dispatch_queue will process any pending
       * events, however not all servers flush on issuing a buffer release
       * event. So, we spam the server with roundtrips as they always cause a
       * client flush.
      if (mg_display_roundtrip_queue(dri2_dpy->mg_dpy,
                                     dri2_surf->mg_queue) < 0)
          return -1;
       */
   }

   if (dri2_surf->mg_back == NULL)
      return -1;

   use_flags = __DRI_IMAGE_USE_SHARE | __DRI_IMAGE_USE_BACKBUFFER;

   if (dri2_dpy->is_different_gpu &&
       dri2_surf->mg_back->linear_copy == NULL) {
      /* The LINEAR modifier should be a perfect alias of the LINEAR use
       * flag; try the new interface first before the old, then fall back. */
      if (dri2_dpy->image->base.version >= 15 &&
           dri2_dpy->image->createImageWithModifiers) {
         uint64_t linear_mod = DRM_FORMAT_MOD_LINEAR;

         dri2_surf->mg_back->linear_copy =
            dri2_dpy->image->createImageWithModifiers(dri2_dpy->dri_screen,
                                                      dri2_surf->base.Width,
                                                      dri2_surf->base.Height,
                                                      linear_dri_image_format,
                                                      &linear_mod,
                                                      1,
                                                      NULL);
      } else {
         dri2_surf->mg_back->linear_copy =
            dri2_dpy->image->createImage(dri2_dpy->dri_screen,
                                         dri2_surf->base.Width,
                                         dri2_surf->base.Height,
                                         linear_dri_image_format,
                                         use_flags |
                                         __DRI_IMAGE_USE_LINEAR,
                                         NULL);
      }
      if (dri2_surf->mg_back->linear_copy == NULL)
          return -1;
   }

   if (dri2_surf->mg_back->dri_image == NULL) {
      /* If our DRIImage implementation does not support
       * createImageWithModifiers, then fall back to the old createImage,
       * and hope it allocates an image which is acceptable to the winsys.
        */
      if (num_modifiers && dri2_dpy->image->base.version >= 15 &&
          dri2_dpy->image->createImageWithModifiers) {
         dri2_surf->mg_back->dri_image =
           dri2_dpy->image->createImageWithModifiers(dri2_dpy->dri_screen,
                                                     dri2_surf->base.Width,
                                                     dri2_surf->base.Height,
                                                     dri_image_format,
                                                     modifiers,
                                                     num_modifiers,
                                                     NULL);
      } else {
         dri2_surf->mg_back->dri_image =
            dri2_dpy->image->createImage(dri2_dpy->dri_screen,
                                         dri2_surf->base.Width,
                                         dri2_surf->base.Height,
                                         dri_image_format,
                                         dri2_dpy->is_different_gpu ?
                                              0 : use_flags,
                                         NULL);
      }

      dri2_surf->mg_back->age = 0;
   }
   if (dri2_surf->mg_back->dri_image == NULL)
      return -1;

   dri2_surf->mg_back->locked = true;

   return 0;
}


static void
back_bo_to_dri_buffer(struct dri2_egl_surface *dri2_surf, __DRIbuffer *buffer)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   __DRIimage *image;
   int name, pitch;

   image = dri2_surf->mg_back->dri_image;

   dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_NAME, &name);
   dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &pitch);

   buffer->attachment = __DRI_BUFFER_BACK_LEFT;
   buffer->name = name;
   buffer->pitch = pitch;
   buffer->cpp = 4;
   buffer->flags = 0;
}

static int
update_buffers(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

#if 0 // VW
   if (dri2_surf->base.Width != dri2_surf->mg_win->width ||
       dri2_surf->base.Height != dri2_surf->mg_win->height) {

      dri2_surf->base.Width  = dri2_surf->mg_win->width;
      dri2_surf->base.Height = dri2_surf->mg_win->height;
      dri2_surf->dx = dri2_surf->mg_win->dx;
      dri2_surf->dy = dri2_surf->mg_win->dy;
   }

   if (dri2_surf->base.Width != dri2_surf->mg_win->attached_width ||
       dri2_surf->base.Height != dri2_surf->mg_win->attached_height) {
      dri2_minigui_release_buffers(dri2_surf);
   }
#endif

   if (get_back_bo(dri2_surf) < 0) {
      _eglError(EGL_BAD_ALLOC, "failed to allocate color buffer");
      return -1;
   }

   /* If we have an extra unlocked buffer at this point, we had to do triple
    * buffering for a while, but now can go back to just double buffering.
    * That means we can free any unlocked buffer now. */
   for (int i = 0; i < ARRAY_SIZE(dri2_surf->mg_color_buffers); i++) {
      if (!dri2_surf->mg_color_buffers[i].locked &&
          dri2_surf->mg_color_buffers[i].mg_buffer) {
         mg_buffer_destroy(dri2_surf->mg_color_buffers[i].mg_buffer);
         dri2_dpy->image->destroyImage(dri2_surf->mg_color_buffers[i].dri_image);
         if (dri2_dpy->is_different_gpu)
            dri2_dpy->image->destroyImage(dri2_surf->mg_color_buffers[i].linear_copy);
         dri2_surf->mg_color_buffers[i].mg_buffer = NULL;
         dri2_surf->mg_color_buffers[i].dri_image = NULL;
         dri2_surf->mg_color_buffers[i].linear_copy = NULL;
      }
   }

   return 0;
}

static int
update_buffers_if_needed(struct dri2_egl_surface *dri2_surf)
{
   if (dri2_surf->mg_back != NULL)
      return 0;

   return update_buffers(dri2_surf);
}

static __DRIbuffer *
dri2_minigui_get_buffers_with_format(__DRIdrawable * driDrawable,
                                int *width, int *height,
                                unsigned int *attachments, int count,
                                int *out_count, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   int i, j;

   if (update_buffers(dri2_surf) < 0)
      return NULL;

   for (i = 0, j = 0; i < 2 * count; i += 2, j++) {
      __DRIbuffer *local;

      switch (attachments[i]) {
      case __DRI_BUFFER_BACK_LEFT:
         back_bo_to_dri_buffer(dri2_surf, &dri2_surf->buffers[j]);
         break;
      default:
         local = dri2_egl_surface_alloc_local_buffer(dri2_surf, attachments[i],
                                                     attachments[i + 1]);

         if (!local) {
            _eglError(EGL_BAD_ALLOC, "failed to allocate local buffer");
            return NULL;
         }
         dri2_surf->buffers[j] = *local;
         break;
      }
   }

   *out_count = j;
   if (j == 0)
      return NULL;

   *width = dri2_surf->base.Width;
   *height = dri2_surf->base.Height;

   return dri2_surf->buffers;
}

static __DRIbuffer *
dri2_minigui_get_buffers(__DRIdrawable * driDrawable,
                    int *width, int *height,
                    unsigned int *attachments, int count,
                    int *out_count, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   unsigned int *attachments_with_format;
   __DRIbuffer *buffer;
   int visual_idx = dri2_minigui_visual_idx_from_fourcc(dri2_surf->mg_format);

   if (visual_idx == -1)
      return NULL;

   attachments_with_format = calloc(count, 2 * sizeof(unsigned int));
   if (!attachments_with_format) {
      *out_count = 0;
      return NULL;
   }

   for (int i = 0; i < count; ++i) {
      attachments_with_format[2*i] = attachments[i];
      attachments_with_format[2*i + 1] = dri2_mg_visuals[visual_idx].bpp;
   }

   buffer =
      dri2_minigui_get_buffers_with_format(driDrawable,
                                      width, height,
                                      attachments_with_format, count,
                                      out_count, loaderPrivate);

   free(attachments_with_format);

   return buffer;
}

static void
dri2_minigui_flush_front_buffer(__DRIdrawable * driDrawable, void *loaderPrivate)
{
   (void) driDrawable;

   /* FIXME: Does EGL support front buffer rendering at all? */

#if 0
   struct dri2_egl_surface *dri2_surf = loaderPrivate;

   dri2WaitGL(dri2_surf);
#else
   (void) loaderPrivate;
#endif
}

static int
dri2_minigui_do_authenticate(struct dri2_egl_display *dri2_dpy, uint32_t id)
{
   /* TODO */
   return EGL_TRUE;
}

static EGLBoolean
dri2_minigui_local_authenticate(struct dri2_egl_display *dri2_dpy)
{
#ifdef HAVE_LIBDRM
   drm_magic_t magic;

   if (drmGetMagic(dri2_dpy->fd, &magic)) {
      _eglLog(_EGL_WARNING, "DRI2: failed to get drm magic");
      return EGL_FALSE;
   }

   if (dri2_minigui_do_authenticate(dri2_dpy, magic) < 0) {
      _eglLog(_EGL_WARNING, "DRI2: failed to authenticate");
      return EGL_FALSE;
   }
#endif
   return EGL_TRUE;
}

static int
dri2_minigui_authenticate(_EGLDisplay *disp, uint32_t id)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   return dri2_minigui_do_authenticate(dri2_dpy, id);
}

static EGLBoolean
dri2_minigui_add_configs_for_visuals(_EGLDriver *drv, _EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   unsigned int format_count[ARRAY_SIZE(dri2_mg_visuals)] = { 0 };
   unsigned int count = 0;
   bool assigned;

   (void)drv;

   for (unsigned i = 0; dri2_dpy->driver_configs[i]; i++) {
      assigned = false;

      for (unsigned j = 0; j < ARRAY_SIZE(dri2_mg_visuals); j++) {
         struct dri2_egl_config *dri2_conf;

         if (!BITSET_TEST(dri2_dpy->mg_formats, j))
            continue;

         dri2_conf = dri2_add_config(disp, dri2_dpy->driver_configs[i],
               count + 1, EGL_WINDOW_BIT, NULL, dri2_mg_visuals[j].rgba_masks);
         if (dri2_conf) {
            if (dri2_conf->base.ConfigID == count + 1)
               count++;
            format_count[j]++;
            assigned = true;
         }
      }

      if (!assigned && dri2_dpy->is_different_gpu) {
         struct dri2_egl_config *dri2_conf;
         int alt_dri_image_format, c, s;

         /* No match for config. Try if we can blitImage convert to a visual */
         c = dri2_minigui_visual_idx_from_config(dri2_dpy,
                                            dri2_dpy->driver_configs[i]);

         if (c == -1)
            continue;

         /* Find optimal target visual for blitImage conversion, if any. */
         alt_dri_image_format = dri2_mg_visuals[c].alt_dri_image_format;
         s = dri2_minigui_visual_idx_from_dri_image_format(alt_dri_image_format);

         if (s == -1 || !BITSET_TEST(dri2_dpy->mg_formats, s))
            continue;

         /* Visual s works for the Wayland server, and c can be converted into s
          * by our client gpu during PRIME blitImage conversion to a linear
          * mg_buffer, so add visual c as supported by the client renderer.
          */
         dri2_conf = dri2_add_config(disp, dri2_dpy->driver_configs[i],
                                     count + 1, EGL_WINDOW_BIT, NULL,
                                     dri2_mg_visuals[c].rgba_masks);
         if (dri2_conf) {
            if (dri2_conf->base.ConfigID == count + 1)
               count++;
            format_count[c]++;
            if (format_count[c] == 1)
               _eglLog(_EGL_DEBUG, "Client format %s to server format %s via "
                       "PRIME blitImage.", dri2_mg_visuals[c].format_name,
                       dri2_mg_visuals[s].format_name);
         }
      }
   }

   for (unsigned i = 0; i < ARRAY_SIZE(format_count); i++) {
      if (!format_count[i]) {
         _eglLog(_EGL_DEBUG, "No DRI config supports native format %s",
                 dri2_mg_visuals[i].format_name);
      }
   }

   return (count != 0);
}

static EGLBoolean
dri2_minigui_swap_buffers(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   if (!dri2_dpy->flush) {
      dri2_dpy->core->swapBuffers(dri2_surf->dri_drawable);
      return EGL_TRUE;
   }

   // VW: TODO
   return EGL_TRUE;
}

static const struct dri2_egl_display_vtbl dri2_minigui_swrast_display_vtbl = {
   .authenticate = NULL,
   .create_window_surface = dri2_minigui_create_window_surface,
   .create_pixmap_surface = dri2_minigui_create_pixmap_surface,
   .create_pbuffer_surface = dri2_fallback_create_pbuffer_surface,
   .destroy_surface = dri2_minigui_destroy_surface,
   .create_image = dri2_create_image_khr,
   .swap_buffers = dri2_minigui_swap_buffers,
   .swap_buffers_region = dri2_fallback_swap_buffers_region,
   .post_sub_buffer = dri2_fallback_post_sub_buffer,
   .copy_buffers = dri2_fallback_copy_buffers,
   .query_buffer_age = dri2_fallback_query_buffer_age,
   .create_wayland_buffer_from_image = dri2_fallback_create_wayland_buffer_from_image,
   .get_sync_values = dri2_fallback_get_sync_values,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static const struct dri2_egl_display_vtbl dri2_minigui_display_vtbl = {
   .authenticate = dri2_minigui_authenticate,
   .create_window_surface = dri2_minigui_create_window_surface,
   .create_pixmap_surface = dri2_minigui_create_pixmap_surface,
   .create_pbuffer_surface = dri2_fallback_create_pbuffer_surface,
   .destroy_surface = dri2_minigui_destroy_surface,
   .create_image = dri2_create_image_khr,
   .swap_buffers = dri2_minigui_swap_buffers,
   .swap_buffers_with_damage = dri2_fallback_swap_buffers_with_damage,
   .swap_buffers_region = dri2_fallback_swap_buffers_region,
   .post_sub_buffer = dri2_fallback_post_sub_buffer,
   .copy_buffers = dri2_fallback_copy_buffers,
   .query_buffer_age = dri2_fallback_query_buffer_age,
   .create_wayland_buffer_from_image = dri2_fallback_create_wayland_buffer_from_image,
   .get_sync_values = dri2_fallback_get_sync_values,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static const __DRIswrastLoaderExtension swrast_loader_extension = {
   .base = { __DRI_SWRAST_LOADER, 2 },

   .getDrawableInfo = dri2_minigui_swrast_get_drawable_info,
   .putImage        = dri2_minigui_swrast_put_image,
   .getImage        = dri2_minigui_swrast_get_image,
   .putImage2       = dri2_minigui_swrast_put_image2,
};

static const __DRIextension *swrast_loader_extensions[] = {
   &swrast_loader_extension.base,
   &image_lookup_extension.base,
   NULL,
};

static EGLBoolean
dri2_initialize_minigui_swrast(_EGLDriver *drv, _EGLDisplay *disp)
{
   _EGLDevice *dev;
   struct dri2_egl_display *dri2_dpy;

   dri2_dpy = calloc(1, sizeof *dri2_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;
   dev = _eglAddDevice(dri2_dpy->fd, true);
   if (!dev) {
      _eglError(EGL_NOT_INITIALIZED, "DRI2: failed to find EGLDevice");
      goto cleanup;
   }

   disp->Device = dev;

   /*
    * Every hardware driver_name is set using strdup. Doing the same in
    * here will allow is to simply free the memory at dri2_terminate().
    */
   dri2_dpy->driver_name = strdup("swrast");
   if (!dri2_load_driver_swrast(disp))
      goto cleanup;

   dri2_dpy->loader_extensions = swrast_loader_extensions;

   if (!dri2_create_screen(disp))
      goto cleanup;

   if (!dri2_setup_extensions(disp))
      goto cleanup;

   dri2_setup_screen(disp);

   if (!dri2_minigui_add_configs_for_visuals(drv, disp))
      goto cleanup;

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &dri2_minigui_swrast_display_vtbl;

   return EGL_TRUE;

 cleanup:
   dri2_display_destroy(disp);
   return EGL_FALSE;
}

static void
dri2_minigui_setup_swap_interval(_EGLDisplay *disp)
{
   /* We can't use values greater than 1 on Wayland because we are using the
    * frame callback to synchronise the frame and the only way we be sure to
    * get a frame callback is to attach a new buffer. Therefore we can't just
    * sit drawing nothing to wait until the next ‘n’ frame callbacks */

   dri2_setup_swap_interval(disp, 1);
}

static const __DRIdri2LoaderExtension dri2_loader_extension_old = {
   .base = { __DRI_DRI2_LOADER, 2 },

   .getBuffers           = dri2_minigui_get_buffers,
   .flushFrontBuffer     = dri2_minigui_flush_front_buffer,
   .getBuffersWithFormat = NULL,
};

static const __DRIdri2LoaderExtension dri2_loader_extension = {
   .base = { __DRI_DRI2_LOADER, 3 },

   .getBuffers           = dri2_minigui_get_buffers,
   .flushFrontBuffer     = dri2_minigui_flush_front_buffer,
   .getBuffersWithFormat = dri2_minigui_get_buffers_with_format,
};

static const __DRIextension *dri2_loader_extensions_old[] = {
   &dri2_loader_extension_old.base,
   &image_lookup_extension.base,
   &background_callable_extension.base,
   NULL,
};

static const __DRIextension *dri2_loader_extensions[] = {
   &dri2_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   &background_callable_extension.base,
   NULL,
};

static EGLBoolean
dri2_initialize_minigui_dri2(_EGLDriver *drv, _EGLDisplay *disp)
{
   _EGLDevice *dev;
   struct dri2_egl_display *dri2_dpy;

   dri2_dpy = calloc(1, sizeof *dri2_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;
#ifdef _MGGAL_DRM
   dri2_dpy->fd = drmGetDeviceFD(dri2_dpy->mg_video);
#endif
   if(dri2_dpy->fd < 0) {
      _eglError(EGL_BAD_DISPLAY, "DRI2: not using MiniGUI DRM engine");
      goto cleanup;
   }

   dev = _eglAddDevice(dri2_dpy->fd, false);
   if (!dev) {
      _eglError(EGL_NOT_INITIALIZED, "DRI2: failed to find EGLDevice");
      goto cleanup;
   }

   disp->Device = dev;

   if (!dri2_load_driver(disp))
      goto cleanup;

   if (dri2_dpy->dri2_minor >= 1)
      dri2_dpy->loader_extensions = dri2_loader_extensions;
   else
      dri2_dpy->loader_extensions = dri2_loader_extensions_old;

   //dri2_dpy->swap_available = (dri2_dpy->dri2_minor >= 2);
   dri2_dpy->invalidate_available = (dri2_dpy->dri2_minor >= 3);

   if (!dri2_create_screen(disp))
      goto cleanup;

   if (!dri2_setup_extensions(disp))
      goto cleanup;

   dri2_dpy->mg_modifiers =
      calloc(ARRAY_SIZE(dri2_mg_visuals), sizeof(*dri2_dpy->mg_modifiers));
   if (!dri2_dpy->mg_modifiers)
      goto cleanup;
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (!u_vector_init(&dri2_dpy->mg_modifiers[i], sizeof(uint64_t), 32))
         goto cleanup;
   }

   dri2_setup_screen(disp);

   dri2_minigui_setup_swap_interval(disp);

   disp->Extensions.KHR_image_pixmap = EGL_TRUE;
   disp->Extensions.NOK_swap_region = EGL_TRUE;
   disp->Extensions.NOK_texture_from_pixmap = EGL_TRUE;
   disp->Extensions.NV_post_sub_buffer = EGL_TRUE;
   disp->Extensions.CHROMIUM_sync_control = EGL_TRUE;

   dri2_set_WL_bind_wayland_display(drv, disp);

   if (!dri2_minigui_add_configs_for_visuals(drv, disp))
      goto cleanup;

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &dri2_minigui_display_vtbl;

   _eglLog(_EGL_INFO, "Using DRI2");

   return EGL_TRUE;

 cleanup:
   dri2_display_destroy(disp);
   return EGL_FALSE;
}

EGLBoolean
dri2_initialize_minigui(_EGLDriver *drv, _EGLDisplay *disp)
{
   EGLBoolean initialized = EGL_FALSE;

   if (!disp->Options.ForceSoftware) {
      initialized = dri2_initialize_minigui_dri2(drv, disp);
   }

   if (!initialized)
      initialized = dri2_initialize_minigui_swrast(drv, disp);

   return initialized;
}

void
dri2_teardown_minigui(struct dri2_egl_display *dri2_dpy)
{
#if 0 // VW
   if (dri2_dpy->mg_drm)
      mg_drm_destroy(dri2_dpy->mg_drm);
   if (dri2_dpy->mg_dmabuf)
      zwp_linux_dmabuf_v1_destroy(dri2_dpy->mg_dmabuf);
   if (dri2_dpy->mg_shm)
      mg_shm_destroy(dri2_dpy->mg_shm);
   if (dri2_dpy->mg_registry)
      mg_registry_destroy(dri2_dpy->mg_registry);
   if (dri2_dpy->mg_queue)
      mg_event_queue_destroy(dri2_dpy->mg_queue);
   if (dri2_dpy->mg_dpy_wrapper)
      mg_proxy_wrapper_destroy(dri2_dpy->mg_dpy_wrapper);
#endif

   for (int i = 0; dri2_dpy->mg_modifiers && i < ARRAY_SIZE(dri2_mg_visuals); i++)
      u_vector_finish(&dri2_dpy->mg_modifiers[i]);
   free(dri2_dpy->mg_modifiers);
}
