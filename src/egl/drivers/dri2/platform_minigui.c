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

#define _DEBUG
#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>

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
/*
 * Define the formats if no fourcc file.
 * We copied the definitions from drm_fourcc.h
 */
#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
                ((__u32)(c) << 16) | ((__u32)(d) << 24))

/* 16 bpp RGB */
#define DRM_FORMAT_XRGB4444    fourcc_code('X', 'R', '1', '2') /* [15:0] x:R:G:B 4:4:4:4 little endian */
#define DRM_FORMAT_XBGR4444    fourcc_code('X', 'B', '1', '2') /* [15:0] x:B:G:R 4:4:4:4 little endian */
#define DRM_FORMAT_RGBX4444    fourcc_code('R', 'X', '1', '2') /* [15:0] R:G:B:x 4:4:4:4 little endian */
#define DRM_FORMAT_BGRX4444    fourcc_code('B', 'X', '1', '2') /* [15:0] B:G:R:x 4:4:4:4 little endian */

#define DRM_FORMAT_ARGB4444    fourcc_code('A', 'R', '1', '2') /* [15:0] A:R:G:B 4:4:4:4 little endian */
#define DRM_FORMAT_ABGR4444    fourcc_code('A', 'B', '1', '2') /* [15:0] A:B:G:R 4:4:4:4 little endian */
#define DRM_FORMAT_RGBA4444    fourcc_code('R', 'A', '1', '2') /* [15:0] R:G:B:A 4:4:4:4 little endian */
#define DRM_FORMAT_BGRA4444    fourcc_code('B', 'A', '1', '2') /* [15:0] B:G:R:A 4:4:4:4 little endian */

#define DRM_FORMAT_XRGB1555    fourcc_code('X', 'R', '1', '5') /* [15:0] x:R:G:B 1:5:5:5 little endian */
#define DRM_FORMAT_XBGR1555    fourcc_code('X', 'B', '1', '5') /* [15:0] x:B:G:R 1:5:5:5 little endian */
#define DRM_FORMAT_RGBX5551    fourcc_code('R', 'X', '1', '5') /* [15:0] R:G:B:x 5:5:5:1 little endian */
#define DRM_FORMAT_BGRX5551    fourcc_code('B', 'X', '1', '5') /* [15:0] B:G:R:x 5:5:5:1 little endian */

#define DRM_FORMAT_ARGB1555    fourcc_code('A', 'R', '1', '5') /* [15:0] A:R:G:B 1:5:5:5 little endian */
#define DRM_FORMAT_ABGR1555    fourcc_code('A', 'B', '1', '5') /* [15:0] A:B:G:R 1:5:5:5 little endian */
#define DRM_FORMAT_RGBA5551    fourcc_code('R', 'A', '1', '5') /* [15:0] R:G:B:A 5:5:5:1 little endian */
#define DRM_FORMAT_BGRA5551    fourcc_code('B', 'A', '1', '5') /* [15:0] B:G:R:A 5:5:5:1 little endian */

#define DRM_FORMAT_RGB565    fourcc_code('R', 'G', '1', '6') /* [15:0] R:G:B 5:6:5 little endian */
#define DRM_FORMAT_BGR565    fourcc_code('B', 'G', '1', '6') /* [15:0] B:G:R 5:6:5 little endian */

/* 24 bpp RGB */
#define DRM_FORMAT_RGB888    fourcc_code('R', 'G', '2', '4') /* [23:0] R:G:B little endian */
#define DRM_FORMAT_BGR888    fourcc_code('B', 'G', '2', '4') /* [23:0] B:G:R little endian */

#define DRM_FORMAT_RGB888    fourcc_code('R', 'G', '2', '4') /* [23:0] R:G:B little endian */
#define DRM_FORMAT_BGR888    fourcc_code('B', 'G', '2', '4') /* [23:0] B:G:R little endian */

/* 32 bpp RGB */
#define DRM_FORMAT_XRGB8888    fourcc_code('X', 'R', '2', '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_XBGR8888    fourcc_code('X', 'B', '2', '4') /* [31:0] x:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBX8888    fourcc_code('R', 'X', '2', '4') /* [31:0] R:G:B:x 8:8:8:8 little endian */
#define DRM_FORMAT_BGRX8888    fourcc_code('B', 'X', '2', '4') /* [31:0] B:G:R:x 8:8:8:8 little endian */

#define DRM_FORMAT_ARGB8888    fourcc_code('A', 'R', '2', '4') /* [31:0] A:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_ABGR8888    fourcc_code('A', 'B', '2', '4') /* [31:0] A:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBA8888    fourcc_code('R', 'A', '2', '4') /* [31:0] R:G:B:A 8:8:8:8 little endian */
#define DRM_FORMAT_BGRA8888    fourcc_code('B', 'A', '2', '4') /* [31:0] B:G:R:A 8:8:8:8 little endian */

#define DRM_FORMAT_XRGB2101010    fourcc_code('X', 'R', '3', '0') /* [31:0] x:R:G:B 2:10:10:10 little endian */
#define DRM_FORMAT_XBGR2101010    fourcc_code('X', 'B', '3', '0') /* [31:0] x:B:G:R 2:10:10:10 little endian */
#define DRM_FORMAT_RGBX1010102    fourcc_code('R', 'X', '3', '0') /* [31:0] R:G:B:x 10:10:10:2 little endian */
#define DRM_FORMAT_BGRX1010102    fourcc_code('B', 'X', '3', '0') /* [31:0] B:G:R:x 10:10:10:2 little endian */

#define DRM_FORMAT_ARGB2101010    fourcc_code('A', 'R', '3', '0') /* [31:0] A:R:G:B 2:10:10:10 little endian */
#define DRM_FORMAT_ABGR2101010    fourcc_code('A', 'B', '3', '0') /* [31:0] A:B:G:R 2:10:10:10 little endian */
#define DRM_FORMAT_RGBA1010102    fourcc_code('R', 'A', '3', '0') /* [31:0] R:G:B:A 10:10:10:2 little endian */
#define DRM_FORMAT_BGRA1010102    fourcc_code('B', 'A', '3', '0') /* [31:0] B:G:R:A 10:10:10:2 little endian */

#endif /* HAVE_LIBDRM */

#define MG_DRM_CAPABILITY_NAME      0x01
#define MG_DRM_CAPABILITY_PRIME     0x02

struct dri2_egl_drv_display
{
   struct dri2_egl_display base;

   GHANDLE                   video;
   struct u_vector          *modifiers;
   BITSET_DECLARE(formats, EGL_DRI2_MAX_FORMATS);
   uint32_t                  capabilities;
};

struct dri2_egl_drv_surface;

typedef void (*CB_RESIZED)(HWND, struct dri2_egl_drv_surface* surf, const RECT* rc_client);
typedef void (*CB_DESTROY)(HWND, struct dri2_egl_drv_surface* surf);

struct dri2_egl_drv_surface
{
   struct dri2_egl_surface base;

   HWND        win;
   WNDPROC     old_wnd_proc;
   CB_RESIZED  cb_resized;
   CB_DESTROY  cb_destroy;
   HDC         private_cdc;
   int         mg_format;

#if 0
   __DRIimage *dri_image_front;
   __DRIimage *dri_image_back;
#endif

   struct {
      HDC               memdc;
      bool              release;
      __DRIimage       *dri_image;
      /* for is_different_gpu case. NULL else */
      __DRIimage       *linear_copy;
      /* for swrast */
      void             *data;
      int               data_size;
      bool              locked;
      int               age;
   } color_buffers[4], *back, *current;
};

/*
 * The index of entries in this table is used as a bitmask in
 * dri2_drv_dpy->formats, which tracks the formats supported by MiniGUI.
 */
static const struct dri2_minigui_visual {
   const char *format_name;
   uint32_t drm_format;
   int dri_image_format;
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
static uint32_t
dri2_minigui_format_from_masks(unsigned int rmask,
        unsigned int gmask, unsigned int bmask, unsigned int amask)
{
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (dri2_mg_visuals[i].rgba_masks[0] == rmask &&
            dri2_mg_visuals[i].rgba_masks[1] == gmask &&
            dri2_mg_visuals[i].rgba_masks[2] == bmask &&
            dri2_mg_visuals[i].rgba_masks[3] == amask)
         return dri2_mg_visuals[i].drm_format;
   }

   return 0;
}

static uint32_t
dri2_minigui_image_format_from_masks(unsigned int rmask,
        unsigned int gmask, unsigned int bmask, unsigned int amask)
{
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (dri2_mg_visuals[i].rgba_masks[0] == rmask &&
            dri2_mg_visuals[i].rgba_masks[1] == gmask &&
            dri2_mg_visuals[i].rgba_masks[2] == bmask &&
            dri2_mg_visuals[i].rgba_masks[3] == amask)
         return dri2_mg_visuals[i].dri_image_format;
   }

   return 0;
}

static int
dri2_minigui_visual_idx_from_shm_format(uint32_t shm_format)
{
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (dri2_mg_visuals[i].shm_format == shm_format)
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

static int
dri2_minigui_swrast_get_stride_for_format(int format, int w, int* visual_idx)
{
   int my_visual_idx = dri2_minigui_visual_idx_from_fourcc(format);

   assume(my_visual_idx != -1);

   if (visual_idx)
      *visual_idx = my_visual_idx;

   return w * (dri2_mg_visuals[my_visual_idx].bpp / 8);
}

static inline void
dri2_minigui_destroy_memdc(HDC dc)
{
    DeleteMemDC(dc);
}

static void
dri2_minigui_release_buffer(struct dri2_egl_drv_surface *dri2_drv_surf, HDC *memdc)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); ++i)
      if (dri2_drv_surf->color_buffers[i].memdc == memdc)
         break;

   assert (i < ARRAY_SIZE(dri2_drv_surf->color_buffers));

   if (dri2_drv_surf->color_buffers[i].release) {
      dri2_drv_surf->color_buffers[i].release = false;
      //dri2_minigui_destroy_memdc(memdc);
      //dri2_drv_surf->color_buffers[i].memdc = NULL;
   }

   dri2_drv_surf->color_buffers[i].locked = false;
}

static EGLBoolean
dri2_minigui_swrast_allocate_buffer(struct dri2_egl_drv_surface *dri2_drv_surf,
                               int format, int w, int h,
                               void **data, int *size, HDC *dc)
{
   int visual_idx, fd, stride, size_map;
   void *data_map;
   HDC memdc;

   stride = dri2_minigui_swrast_get_stride_for_format(format, w, &visual_idx);
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

   memdc = CreateMemDCEx (w, h, dri2_mg_visuals[visual_idx].bpp,
        MEMDC_FLAG_SWSURFACE,
        dri2_mg_visuals[visual_idx].rgba_masks[0],
        dri2_mg_visuals[visual_idx].rgba_masks[1],
        dri2_mg_visuals[visual_idx].rgba_masks[2],
        dri2_mg_visuals[visual_idx].rgba_masks[3],
        data_map, stride);

   if (memdc == HDC_INVALID) {
      munmap(data_map, size_map);
      close(fd);
      return EGL_FALSE;
   }

   /* close fd here */
   close(fd);

   *data = data_map;
   *size = size_map;
   *dc = memdc;

   _eglLog(_EGL_DEBUG, "a software memdc created: w(%d), h(%d), stride(%d), visual(%d)\n",
            w, h, stride, visual_idx);

   return EGL_TRUE;
}

static void
dri2_minigui_release_buffers(struct dri2_egl_drv_surface *dri2_drv_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_drv_surf->base.base.Resource.Display);

   for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++) {
      if (dri2_drv_surf->color_buffers[i].memdc) {
         if (dri2_drv_surf->color_buffers[i].locked) {
            dri2_drv_surf->color_buffers[i].release = true;
         } else {
            dri2_minigui_destroy_memdc(dri2_drv_surf->color_buffers[i].memdc);
            dri2_drv_surf->color_buffers[i].memdc = NULL;
         }
      }
      if (dri2_drv_surf->color_buffers[i].dri_image)
         dri2_dpy->image->destroyImage(dri2_drv_surf->color_buffers[i].dri_image);
      if (dri2_drv_surf->color_buffers[i].linear_copy)
         dri2_dpy->image->destroyImage(dri2_drv_surf->color_buffers[i].linear_copy);
      if (dri2_drv_surf->color_buffers[i].data)
         munmap(dri2_drv_surf->color_buffers[i].data,
                dri2_drv_surf->color_buffers[i].data_size);

      dri2_drv_surf->color_buffers[i].dri_image = NULL;
      dri2_drv_surf->color_buffers[i].linear_copy = NULL;
      dri2_drv_surf->color_buffers[i].data = NULL;
   }

   if (dri2_dpy->dri2)
      dri2_egl_surface_free_local_buffers(&dri2_drv_surf->base);
}


static int
swrast_update_buffers(struct dri2_egl_drv_surface *dri2_drv_surf)
{
#if 0 // VW
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
#endif
   RECT rc_win;

   /* we need to do the following operations only once per frame */
   if (dri2_drv_surf->back)
      return 0;

   GetClientRect(dri2_drv_surf->win, &rc_win);
   if (dri2_drv_surf->base.base.Width != RECTW(rc_win) ||
       dri2_drv_surf->base.base.Height != RECTH(rc_win)) {

      dri2_minigui_release_buffers(dri2_drv_surf);

      dri2_drv_surf->base.base.Width  = RECTW(rc_win);
      dri2_drv_surf->base.base.Height = RECTH(rc_win);
      dri2_drv_surf->current = NULL;
   }

   /* find back buffer */

   /* try get free buffer already created */
   for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++) {
      if (!dri2_drv_surf->color_buffers[i].locked &&
          dri2_drv_surf->color_buffers[i].memdc) {
          dri2_drv_surf->back = &dri2_drv_surf->color_buffers[i];
          break;
      }
   }

   /* else choose any another free location */
   if (!dri2_drv_surf->back) {
      for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++) {
         if (!dri2_drv_surf->color_buffers[i].locked) {
             dri2_drv_surf->back = &dri2_drv_surf->color_buffers[i];
             if (!dri2_minigui_swrast_allocate_buffer(dri2_drv_surf,
                                                 dri2_drv_surf->mg_format,
                                                 dri2_drv_surf->base.base.Width,
                                                 dri2_drv_surf->base.base.Height,
                                                 &dri2_drv_surf->back->data,
                                                 &dri2_drv_surf->back->data_size,
                                                 &dri2_drv_surf->back->memdc)) {
                 _eglError(EGL_BAD_ALLOC, "failed to allocate color buffer");
                 return -1;
             }
             break;
         }
      }
   }

   if (!dri2_drv_surf->back) {
      _eglError(EGL_BAD_ALLOC, "failed to find free buffer");
      return -1;
   }

   dri2_drv_surf->back->locked = true;

   /* If we have an extra unlocked buffer at this point, we had to do triple
    * buffering for a while, but now can go back to just double buffering.
    * That means we can free any unlocked buffer now. */
   for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++) {
      if (!dri2_drv_surf->color_buffers[i].locked &&
          dri2_drv_surf->color_buffers[i].memdc) {
         dri2_minigui_destroy_memdc(dri2_drv_surf->color_buffers[i].memdc);
         munmap(dri2_drv_surf->color_buffers[i].data,
                dri2_drv_surf->color_buffers[i].data_size);
         dri2_drv_surf->color_buffers[i].memdc = NULL;
         dri2_drv_surf->color_buffers[i].data = NULL;
      }
   }

   return 0;
}

static void*
dri2_minigui_swrast_get_frontbuffer_data(struct dri2_egl_drv_surface *dri2_drv_surf)
{
   /* if there has been a resize: */
   if (!dri2_drv_surf->current)
      return NULL;

   return dri2_drv_surf->current->data;
}

static void*
dri2_minigui_swrast_get_backbuffer_data(struct dri2_egl_drv_surface *dri2_drv_surf)
{
   assert(dri2_drv_surf->back);
   return dri2_drv_surf->back->data;
}

static void
dri2_minigui_swrast_commit_backbuffer(struct dri2_egl_drv_surface *dri2_drv_surf)
{
   dri2_drv_surf->current = dri2_drv_surf->back;
   dri2_drv_surf->back = NULL;

   SelectClipRect(dri2_drv_surf->private_cdc, NULL);
   BitBlt(dri2_drv_surf->current->memdc, 0, 0, 0, 0,
            dri2_drv_surf->private_cdc, 0, 0, 0);

   dri2_minigui_release_buffer(dri2_drv_surf, dri2_drv_surf->current->memdc);
}

static void
dri2_minigui_swrast_get_drawable_info(__DRIdrawable * draw,
                                 int *x, int *y, int *w, int *h,
                                 void *loaderPrivate)
{
   struct dri2_egl_drv_surface *dri2_drv_surf = loaderPrivate;

   (void) swrast_update_buffers(dri2_drv_surf);
   *x = 0;
   *y = 0;
   *w = dri2_drv_surf->base.base.Width;
   *h = dri2_drv_surf->base.base.Height;
}

static void
dri2_minigui_swrast_get_image(__DRIdrawable * read,
                         int x, int y, int w, int h,
                         char *data, void *loaderPrivate)
{
   struct dri2_egl_drv_surface *dri2_drv_surf = loaderPrivate;
   struct dri2_egl_surface *dri2_surf = &dri2_drv_surf->base;
   int copy_width = dri2_minigui_swrast_get_stride_for_format(dri2_drv_surf->mg_format,
            w, NULL);
   int x_offset = dri2_minigui_swrast_get_stride_for_format(dri2_drv_surf->mg_format,
            x, NULL);
   int src_stride = dri2_minigui_swrast_get_stride_for_format(dri2_drv_surf->mg_format,
            dri2_surf->base.Width, NULL);
   int dst_stride = copy_width;
   char *src, *dst;

   src = dri2_minigui_swrast_get_frontbuffer_data(dri2_drv_surf);
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
   struct dri2_egl_drv_surface *dri2_drv_surf = loaderPrivate;
   int copy_width = dri2_minigui_swrast_get_stride_for_format(dri2_drv_surf->mg_format,
            w, NULL);
   int dst_stride = dri2_minigui_swrast_get_stride_for_format(dri2_drv_surf->mg_format,
            dri2_drv_surf->base.base.Width, NULL);
   int x_offset = dri2_minigui_swrast_get_stride_for_format(dri2_drv_surf->mg_format,
            x, NULL);
   char *src, *dst;

   assert(copy_width <= stride);

   (void) swrast_update_buffers(dri2_drv_surf);
   dst = dri2_minigui_swrast_get_backbuffer_data(dri2_drv_surf);

   /* partial copy, copy old content */
   if (copy_width < dst_stride) {
      dri2_minigui_swrast_get_image(draw, 0, 0,
                               dri2_drv_surf->base.base.Width,
                               dri2_drv_surf->base.base.Height,
                               dst, loaderPrivate);
   }

   dst += x_offset;
   dst += y * dst_stride;

   src = data;

   /* drivers expect we do these checks (and some rely on it) */
   if (copy_width > dst_stride-x_offset)
      copy_width = dst_stride-x_offset;
   if (h > dri2_drv_surf->base.base.Height-y)
      h = dri2_drv_surf->base.base.Height-y;

   for (; h>0; h--) {
      memcpy(dst, src, copy_width);
      src += stride;
      dst += dst_stride;
   }

   dri2_minigui_swrast_commit_backbuffer(dri2_drv_surf);
}

static void
dri2_minigui_swrast_put_image(__DRIdrawable * draw, int op,
                         int x, int y, int w, int h,
                         char *data, void *loaderPrivate)
{
   struct dri2_egl_drv_surface *dri2_drv_surf = loaderPrivate;
   int stride;

   stride = dri2_minigui_swrast_get_stride_for_format(dri2_drv_surf->mg_format,
                             w, NULL);
   dri2_minigui_swrast_put_image2(draw, op, x, y, w, h,
                             stride, data, loaderPrivate);
}

static void
window_resized_callback (HWND hwnd, struct dri2_egl_drv_surface *dri2_drv_surf,
        const RECT* rc_client)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_drv_surf->base.base.Resource.Display);

   /* Update the surface size as soon as native window is resized; from user
    * pov, this makes the effect that resize is done immediately after native
    * window resize, without requiring to wait until the first draw.
    *
    * A more detailed and lengthy explanation can be found at
    * https://lists.freedesktop.org/archives/mesa-dev/2018-June/196474.html
    */
   if (!dri2_drv_surf->back) {
      dri2_drv_surf->base.base.Width = RECTWP(rc_client);
      dri2_drv_surf->base.base.Height = RECTHP(rc_client);
   }

   if (dri2_dpy->flush)
      dri2_dpy->flush->invalidate(dri2_drv_surf->base.dri_drawable);
}

static void
destroy_window_callback (HWND hwnd, struct dri2_egl_drv_surface *dri2_drv_surf)
{
   dri2_drv_surf->win = HWND_NULL;
}

static LRESULT egl_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
   struct dri2_egl_drv_surface *dri2_drv_surf;

   dri2_drv_surf = (struct dri2_egl_drv_surface *)GetWindowAdditionalData2 (hwnd);
   switch (msg) {
   case MSG_SIZECHANGED:
      if (dri2_drv_surf->cb_resized) {
         dri2_drv_surf->cb_resized (hwnd, dri2_drv_surf, (const RECT*)lparam);
      }
      break;

   case MSG_DESTROY:
      if (dri2_drv_surf->cb_destroy) {
         dri2_drv_surf->cb_destroy (hwnd, dri2_drv_surf);
      }
      break;
   }

   return dri2_drv_surf->old_wnd_proc (hwnd, msg, wparam, lparam);
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
   struct dri2_egl_drv_surface *dri2_drv_surf;
   const __DRIconfig *config;
   RECT win_cli_rc;
   int visual_idx;

   (void) drv;

   dri2_drv_surf = calloc(1, sizeof *dri2_drv_surf);
   if (!dri2_drv_surf) {
      _eglError(EGL_BAD_ALLOC, "dri2_create_window_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_drv_surf->base.base, disp, EGL_WINDOW_BIT, conf,
                            attrib_list, false, native_window))
      goto cleanup_surf;

   config = dri2_get_dri_config(dri2_conf, EGL_WINDOW_BIT,
                                dri2_drv_surf->base.base.GLColorspace);

   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_pixmap;
   }

   dri2_drv_surf->win = (HWND)native_window;
   GetClientRect (dri2_drv_surf->win, &win_cli_rc);
   dri2_drv_surf->base.base.Width = RECTW(win_cli_rc);
   dri2_drv_surf->base.base.Height = RECTH(win_cli_rc);

   visual_idx = dri2_minigui_visual_idx_from_config(dri2_dpy, config);
   assert(visual_idx != -1);
   dri2_drv_surf->mg_format = dri2_mg_visuals[visual_idx].drm_format;

   SetWindowAdditionalData2(dri2_drv_surf->win, (DWORD)dri2_drv_surf);
   dri2_drv_surf->old_wnd_proc = GetWindowCallbackProc(dri2_drv_surf->win);
   SetWindowCallbackProc(dri2_drv_surf->win, egl_window_proc);

   dri2_drv_surf->private_cdc = GetPrivateClientDC(dri2_drv_surf->win);
   if (!dri2_drv_surf->private_cdc) {
      dri2_drv_surf->private_cdc = CreatePrivateClientDC(dri2_drv_surf->win);
   }

   dri2_drv_surf->cb_destroy = destroy_window_callback;
   if (dri2_dpy->flush)
      dri2_drv_surf->cb_resized = window_resized_callback;

   if (!dri2_create_drawable(dri2_dpy, config, &dri2_drv_surf->base,
            &dri2_drv_surf->base)) {
      _eglError(EGL_BAD_MATCH, "failed to create dri_drawable");
      goto cleanup_pixmap;
   }

   dri2_drv_surf->base.base.SwapInterval = dri2_dpy->default_swap_interval;

   return &dri2_drv_surf->base.base;

 cleanup_pixmap:
 cleanup_surf:
   free(dri2_drv_surf);

   return NULL;
}

static _EGLSurface *
dri2_minigui_create_pixmap_surface(_EGLDriver *drv, _EGLDisplay *disp,
                               _EGLConfig *conf, void *native_pixmap,
                               const EGLint *attrib_list)
{
#if 1
   /* From the EGL_EXT_platform_minigui spec, version 1:
    *
    *   It is not valid to call eglCreatePlatformPixmapSurfaceEXT with a <dpy>
    *   that belongs to Wayland. Any such call fails and generates
    *   EGL_BAD_PARAMETER.
    */
   _eglError(EGL_BAD_PARAMETER, "cannot create EGL pixmap surfaces on "
             "MiniGUI");
   return NULL;
#else
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *config;
   DrmSurfaceInfo info;
   int visual_idx;

   (void) drv;

   if (!IsMemDC((HDC)native_pixmap)) {
      _eglError(EGL_BAD_PARAMETER,
            "create_pixmap_surface: not a memory dc");
      return NULL;
   }

   dri2_surf = calloc(1, sizeof *dri2_surf);
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "create_pixmap_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, EGL_PIXMAP_BIT, conf, attrib_list,
                          false, native_pixmap))
      goto cleanup_surf;

   dri2_drv_surf->pixmap = (HDC)native_pixmap;

   config = dri2_get_dri_config(dri2_conf, EGL_PIXMAP_BIT,
                                dri2_surf->base.GLColorspace);
   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surf;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_surf;

   if (dri2_dpy->dri2) {
      // get and bind to the direct buffer object.
      if (!drmGetSurfaceInfo(dri2_drv_dpy->video, (HDC)native_pixmap, &info)) {
         _eglError(EGL_BAD_MATCH, "Not a DRM surface");
         goto cleanup_dri_drawable;
      }

      dri2_surf->base.Width = info.width;
      dri2_surf->base.Height = info.height;
      dri2_drv_surf->mg_format = info.drm_format;
      visual_idx = dri2_minigui_visual_idx_from_fourcc(dri2_drv_surf->mg_format);
      if (visual_idx < 0) {
         _eglError(EGL_BAD_MATCH, "Unsupported colorspace configuration");
         goto cleanup_dri_drawable;
      }
   }
   else {
      dri2_surf->base.Width = GetGDCapability((HDC)native_pixmap, GDCAP_HPIXEL);
      dri2_surf->base.Height = GetGDCapability((HDC)native_pixmap, GDCAP_VPIXEL);
      dri2_drv_surf->mg_format = dri2_minigui_format_from_masks (
                    GetGDCapability((HDC)native_pixmap, GDCAP_RMASK),
                    GetGDCapability((HDC)native_pixmap, GDCAP_GMASK),
                    GetGDCapability((HDC)native_pixmap, GDCAP_BMASK),
                    GetGDCapability((HDC)native_pixmap, GDCAP_AMASK));
      visual_idx = dri2_minigui_visual_idx_from_fourcc(dri2_drv_surf->mg_format);
      if (visual_idx < 0) {
         _eglError(EGL_BAD_MATCH, "Unsupported colorspace configuration");
         goto cleanup_dri_drawable;
      }
   }

   return &dri2_surf->base;

 cleanup_dri_drawable:
   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
 cleanup_surf:
   free(dri2_surf);

   return NULL;
#endif
}

static _EGLSurface *
dri2_minigui_create_pbuffer_surface(_EGLDriver *drv, _EGLDisplay *disp,
                               _EGLConfig *conf, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *config;

   (void) drv;

   dri2_surf = calloc(1, sizeof (struct dri2_egl_drv_surface));
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "create_pbuffer_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, EGL_PBUFFER_BIT,
                            conf, attrib_list, false, NULL))
      goto cleanup_surf;

   config = dri2_get_dri_config(dri2_conf, EGL_PIXMAP_BIT,
                                dri2_surf->base.GLColorspace);
   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surf;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_surf;

   return &dri2_surf->base;

 cleanup_surf:
   free(dri2_surf);

   return NULL;
}

static EGLBoolean
dri2_minigui_destroy_surface(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   struct dri2_egl_drv_surface *dri2_drv_surf = (struct dri2_egl_drv_surface *)dri2_surf;

   (void) drv;

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);

   for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++) {
      if (dri2_drv_surf->color_buffers[i].memdc)
         dri2_minigui_destroy_memdc(dri2_drv_surf->color_buffers[i].memdc);
      if (dri2_drv_surf->color_buffers[i].dri_image)
         dri2_dpy->image->destroyImage(dri2_drv_surf->color_buffers[i].dri_image);
      if (dri2_drv_surf->color_buffers[i].linear_copy)
         dri2_dpy->image->destroyImage(dri2_drv_surf->color_buffers[i].linear_copy);
      if (dri2_drv_surf->color_buffers[i].data)
         munmap(dri2_drv_surf->color_buffers[i].data,
                dri2_drv_surf->color_buffers[i].data_size);
   }

   if (dri2_dpy->dri2)
      dri2_egl_surface_free_local_buffers(dri2_surf);

   if (dri2_drv_surf->win) {
      dri2_drv_surf->cb_resized = NULL;
      dri2_drv_surf->cb_destroy = NULL;
   }

   dri2_fini_surface(surf);
   free(surf);

   return EGL_TRUE;
}

static int
get_back_bo(struct dri2_egl_drv_surface *dri2_drv_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_drv_surf->base.base.Resource.Display);
   struct dri2_egl_drv_display *dri2_drv_dpy =
      (struct dri2_egl_drv_display *)dri2_dpy;
   int use_flags;
   int visual_idx;
   unsigned int dri_image_format;
   unsigned int linear_dri_image_format;
   uint64_t *modifiers;
   int num_modifiers;

   visual_idx = dri2_minigui_visual_idx_from_fourcc(dri2_drv_surf->mg_format);
   assert(visual_idx != -1);
   dri_image_format = dri2_mg_visuals[visual_idx].dri_image_format;
   linear_dri_image_format = dri_image_format;
   modifiers = u_vector_tail(&dri2_drv_dpy->modifiers[visual_idx]);
   num_modifiers = u_vector_length(&dri2_drv_dpy->modifiers[visual_idx]);

   /* Substitute dri image format if server does not support original format */
   if (!BITSET_TEST(dri2_drv_dpy->formats, visual_idx))
      linear_dri_image_format = dri2_mg_visuals[visual_idx].alt_dri_image_format;

   /* These asserts hold, as long as dri2_mg_visuals[] is self-consistent and
    * the PRIME substitution logic in dri2_minigui_add_configs_for_visuals() is free
    * of bugs.
    */
   assert(linear_dri_image_format != __DRI_IMAGE_FORMAT_NONE);
   assert(BITSET_TEST(dri2_drv_dpy->formats,
          dri2_minigui_visual_idx_from_dri_image_format(linear_dri_image_format)));

   while (dri2_drv_surf->back == NULL) {
      for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++) {
         /* Get an unlocked buffer, preferably one with a dri_buffer
          * already allocated. */
         if (dri2_drv_surf->color_buffers[i].locked)
            continue;
         if (dri2_drv_surf->back == NULL)
            dri2_drv_surf->back = &dri2_drv_surf->color_buffers[i];
         else if (dri2_drv_surf->back->dri_image == NULL)
            dri2_drv_surf->back = &dri2_drv_surf->color_buffers[i];
      }

      if (dri2_drv_surf->back)
         break;
   }

   if (dri2_drv_surf->back == NULL)
      return -1;

   use_flags = __DRI_IMAGE_USE_SHARE | __DRI_IMAGE_USE_BACKBUFFER;

   if (dri2_dpy->is_different_gpu &&
       dri2_drv_surf->back->linear_copy == NULL) {
      /* The LINEAR modifier should be a perfect alias of the LINEAR use
       * flag; try the new interface first before the old, then fall back. */
      if (dri2_dpy->image->base.version >= 15 &&
           dri2_dpy->image->createImageWithModifiers) {
         uint64_t linear_mod = DRM_FORMAT_MOD_LINEAR;

         dri2_drv_surf->back->linear_copy =
            dri2_dpy->image->createImageWithModifiers(dri2_dpy->dri_screen,
                                                      dri2_drv_surf->base.base.Width,
                                                      dri2_drv_surf->base.base.Height,
                                                      linear_dri_image_format,
                                                      &linear_mod,
                                                      1,
                                                      NULL);
      } else {
         dri2_drv_surf->back->linear_copy =
            dri2_dpy->image->createImage(dri2_dpy->dri_screen,
                                         dri2_drv_surf->base.base.Width,
                                         dri2_drv_surf->base.base.Height,
                                         linear_dri_image_format,
                                         use_flags |
                                         __DRI_IMAGE_USE_LINEAR,
                                         NULL);
      }
      if (dri2_drv_surf->back->linear_copy == NULL)
          return -1;
   }

   if (dri2_drv_surf->back->dri_image == NULL) {
      /* If our DRIImage implementation does not support
       * createImageWithModifiers, then fall back to the old createImage,
       * and hope it allocates an image which is acceptable to the winsys.
        */
      if (num_modifiers && dri2_dpy->image->base.version >= 15 &&
          dri2_dpy->image->createImageWithModifiers) {
         dri2_drv_surf->back->dri_image =
           dri2_dpy->image->createImageWithModifiers(dri2_dpy->dri_screen,
                                                     dri2_drv_surf->base.base.Width,
                                                     dri2_drv_surf->base.base.Height,
                                                     dri_image_format,
                                                     modifiers,
                                                     num_modifiers,
                                                     NULL);
      } else {
         dri2_drv_surf->back->dri_image =
            dri2_dpy->image->createImage(dri2_dpy->dri_screen,
                                         dri2_drv_surf->base.base.Width,
                                         dri2_drv_surf->base.base.Height,
                                         dri_image_format,
                                         dri2_dpy->is_different_gpu ?
                                              0 : (use_flags | __DRI_IMAGE_USE_LINEAR),
                                         NULL);
      }

      dri2_drv_surf->back->age = 0;
   }

   if (dri2_drv_surf->back->dri_image == NULL)
      return -1;

   dri2_drv_surf->back->locked = true;

   return 0;
}


static void
back_bo_to_dri_buffer(struct dri2_egl_drv_surface *dri2_drv_surf, __DRIbuffer *buffer)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_drv_surf->base.base.Resource.Display);
   __DRIimage *image;
   int name, pitch;

   image = dri2_drv_surf->back->dri_image;

   dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_NAME, &name);
   dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &pitch);

   buffer->attachment = __DRI_BUFFER_BACK_LEFT;
   buffer->name = name;
   buffer->pitch = pitch;
   buffer->cpp = 4;
   buffer->flags = 0;
}

static int
update_buffers(struct dri2_egl_drv_surface *dri2_drv_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_drv_surf->base.base.Resource.Display);
   RECT rc_win;

   GetClientRect (dri2_drv_surf->win, &rc_win);
   if (dri2_drv_surf->base.base.Width != RECTW(rc_win) ||
       dri2_drv_surf->base.base.Height != RECTH(rc_win)) {

      dri2_drv_surf->base.base.Width  = RECTW(rc_win);
      dri2_drv_surf->base.base.Height = RECTH(rc_win);
      dri2_minigui_release_buffers(dri2_drv_surf);
   }

   if (get_back_bo(dri2_drv_surf) < 0) {
      _eglError(EGL_BAD_ALLOC, "failed to allocate color buffer");
      return -1;
   }

   /* If we have an extra unlocked buffer at this point, we had to do triple
    * buffering for a while, but now can go back to just double buffering.
    * That means we can free any unlocked buffer now. */
   for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++) {
      if (!dri2_drv_surf->color_buffers[i].locked &&
          dri2_drv_surf->color_buffers[i].memdc) {
         dri2_minigui_destroy_memdc(dri2_drv_surf->color_buffers[i].memdc);
         dri2_dpy->image->destroyImage(dri2_drv_surf->color_buffers[i].dri_image);
         if (dri2_dpy->is_different_gpu)
            dri2_dpy->image->destroyImage(dri2_drv_surf->color_buffers[i].linear_copy);
         dri2_drv_surf->color_buffers[i].memdc = NULL;
         dri2_drv_surf->color_buffers[i].dri_image = NULL;
         dri2_drv_surf->color_buffers[i].linear_copy = NULL;
      }
   }

   return 0;
}

static int
update_buffers_if_needed(struct dri2_egl_drv_surface *dri2_drv_surf)
{
   if (dri2_drv_surf->back != NULL)
      return 0;

   return update_buffers(dri2_drv_surf);
}

static __DRIbuffer *
dri2_minigui_get_buffers_with_format(__DRIdrawable * driDrawable,
                                int *width, int *height,
                                unsigned int *attachments, int count,
                                int *out_count, void *loaderPrivate)
{
   struct dri2_egl_drv_surface *dri2_drv_surf = loaderPrivate;
   int i, j;

   if (update_buffers(dri2_drv_surf) < 0)
      return NULL;

   for (i = 0, j = 0; i < 2 * count; i += 2, j++) {
      __DRIbuffer *local;

      switch (attachments[i]) {
      case __DRI_BUFFER_BACK_LEFT:
         back_bo_to_dri_buffer(dri2_drv_surf, &dri2_drv_surf->base.buffers[j]);
         break;
      default:
         local = dri2_egl_surface_alloc_local_buffer(&dri2_drv_surf->base, attachments[i],
                                                     attachments[i + 1]);

         if (!local) {
            _eglError(EGL_BAD_ALLOC, "failed to allocate local buffer");
            return NULL;
         }
         dri2_drv_surf->base.buffers[j] = *local;
         break;
      }
   }

   *out_count = j;
   if (j == 0)
      return NULL;

   *width = dri2_drv_surf->base.base.Width;
   *height = dri2_drv_surf->base.base.Height;

   return dri2_drv_surf->base.buffers;
}

static __DRIbuffer *
dri2_minigui_get_buffers(__DRIdrawable * driDrawable,
                    int *width, int *height,
                    unsigned int *attachments, int count,
                    int *out_count, void *loaderPrivate)
{
   struct dri2_egl_drv_surface *dri2_drv_surf = loaderPrivate;
   unsigned int *attachments_with_format;
   __DRIbuffer *buffer;
   int visual_idx = dri2_minigui_visual_idx_from_fourcc(dri2_drv_surf->mg_format);

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

static int
image_get_buffers(__DRIdrawable *driDrawable,
                  unsigned int format,
                  uint32_t *stamp,
                  void *loaderPrivate,
                  uint32_t buffer_mask,
                  struct __DRIimageList *buffers)
{
   struct dri2_egl_drv_surface *dri2_drv_surf = loaderPrivate;

   if (update_buffers(dri2_drv_surf) < 0)
      return 0;

   buffers->image_mask = __DRI_IMAGE_BUFFER_BACK;
   buffers->back = dri2_drv_surf->back->dri_image;

   return 1;
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

#ifdef HAVE_LIBDRM
static int
dri2_minigui_do_authenticate(struct dri2_egl_display *dri2_dpy, uint32_t id)
{
   drm_magic_t magic;

   if (drmGetMagic(dri2_dpy->fd, &magic)) {
      _eglLog(_EGL_WARNING, "DRI2: failed to get drm magic");
      return EGL_FALSE;
   }

   if (dri2_minigui_do_authenticate(dri2_dpy, magic) < 0) {
      _eglLog(_EGL_WARNING, "DRI2: failed to authenticate");
      return EGL_FALSE;
   }

   return EGL_TRUE;
}

#else

static int
dri2_minigui_do_authenticate(struct dri2_egl_display *dri2_dpy, uint32_t id)
{
   return EGL_TRUE;
}

#endif /* HAVE_LIBDRM */

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
   struct dri2_egl_drv_display *dri2_drv_dpy =
        (struct dri2_egl_drv_display *)dri2_dpy;
   unsigned int format_count[ARRAY_SIZE(dri2_mg_visuals)] = { 0 };
   unsigned int count = 0;
   bool assigned;

   (void)drv;

   for (unsigned i = 0; dri2_dpy->driver_configs[i]; i++) {
      assigned = false;

      for (unsigned j = 0; j < ARRAY_SIZE(dri2_mg_visuals); j++) {
         struct dri2_egl_config *dri2_conf;

         if (!BITSET_TEST(dri2_drv_dpy->formats, j))
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

         if (s == -1 || !BITSET_TEST(dri2_drv_dpy->formats, s))
            continue;

         /* Visual s works for the Wayland server, and c can be converted into s
          * by our client gpu during PRIME blitImage conversion to a linear
          * memdc, so add visual c as supported by the client renderer.
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
get_fourcc(struct dri2_egl_display *dri2_dpy,
           __DRIimage *image, int *fourcc)
{
   EGLBoolean query;
   int dri_format;
   int visual_idx;

   query = dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_FOURCC,
                                       fourcc);
   if (query)
      return true;

   query = dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_FORMAT,
                                       &dri_format);
   if (!query)
      return false;

   visual_idx = dri2_minigui_visual_idx_from_dri_image_format(dri_format);
   if (visual_idx == -1)
      return false;

   *fourcc = dri2_mg_visuals[visual_idx].drm_format;
   return true;
}

static HDC
create_minigui_buffer(struct dri2_egl_drv_display *dri2_drv_dpy,
                 struct dri2_egl_drv_surface *dri2_drv_surf,
                 __DRIimage *image)
{
   HDC ret;
   EGLBoolean query;
   int width, height, fourcc, num_planes;

   query = dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_WIDTH, &width);
   query &= dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_HEIGHT, &height);
   query &= get_fourcc(&dri2_drv_dpy->base, image, &fourcc);
   if (!query)
      return NULL;

   query = dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_NUM_PLANES,
                                       &num_planes);
   if (!query)
      num_planes = 1;

   if (num_planes > 1)
      return NULL;

#if 0
   _DBG_PRINTF("num_planes (%d)\n", num_planes);
   if (num_planes == 1) {
       if (dri2_drv_dpy->base.image->fromPlanar) {
          image = dri2_drv_dpy->base.image->fromPlanar(image, 0, image);
       }
       else {
          _DBG_PRINTF("no fromPlanar method\n");
       }
   }
#endif

   if (dri2_drv_dpy->capabilities & MG_DRM_CAPABILITY_PRIME) {
      int fd, stride;

      // FIXME: no size info
      dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_FD, &fd);
      dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &stride);
      ret = drmCreateDCFromPrimeFd(dri2_drv_dpy->video, fd, 0, fourcc,
                                        width, height, stride);
      close(fd);
   }
   else if (dri2_drv_dpy->capabilities & MG_DRM_CAPABILITY_NAME) {
      int name, stride;

      dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_NAME, &name);
      dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &stride);
      ret = drmCreateDCFromName(dri2_drv_dpy->video, name, fourcc,
                width, height, stride);
   }
   else {

      int handle, stride, size_map, offset;

      dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_HANDLE, &handle);
      dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &stride);
      dri2_drv_dpy->base.image->queryImage(image, __DRI_IMAGE_ATTRIB_OFFSET, &offset);

      // FIXME: calculate the size
      size_map = stride * height;
      _eglLog(_EGL_DEBUG, "Image width(%d), height(%d), stride(%d), offset(%d)",
                    width, height, stride, offset);

      ret = drmCreateDCFromHandle(dri2_drv_dpy->video, handle, size_map, fourcc,
                width, height, stride);

      /* create dumb map */
      if (ret == HDC_INVALID) {
         int visual_idx;
         struct drm_mode_map_dumb map_arg;
         void* map;

         visual_idx = dri2_minigui_visual_idx_from_fourcc(fourcc);

         memset(&map_arg, 0, sizeof(map_arg));
         map_arg.handle = handle;

         if (drmIoctl(dri2_drv_dpy->base.fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg)) {
            _eglError(EGL_BAD_ALLOC, "failed to ioctl(DRM_IOCTL_MODE_MAP_DUMB)");
            return NULL;
         }

         map = mmap(0, size_map, PROT_READ | PROT_WRITE,
                  MAP_SHARED, dri2_drv_dpy->base.fd, map_arg.offset);
         if (map == MAP_FAILED) {
            _eglError(EGL_BAD_ALLOC, "failed to call mmap");
            return NULL;
         }

         ret = CreateMemDCEx (width, height, dri2_mg_visuals[visual_idx].bpp,
                MEMDC_FLAG_SWSURFACE,
                dri2_mg_visuals[visual_idx].rgba_masks[0],
                dri2_mg_visuals[visual_idx].rgba_masks[1],
                dri2_mg_visuals[visual_idx].rgba_masks[2],
                dri2_mg_visuals[visual_idx].rgba_masks[3],
                map, stride);

         if (ret == HDC_INVALID) {
            munmap(map, size_map);
            _eglError(EGL_BAD_ALLOC, "failed to create dumb mapped memory DC");
            return NULL;
         }

         dri2_drv_surf->current->data = map;
         dri2_drv_surf->current->data_size = size_map;
      }
   }

   return ret;
}

static EGLBoolean
clip_minigui_buffer(struct dri2_egl_drv_surface *dri2_drv_surf,
            const EGLint *rects, EGLint n_rects)
{
   assert(n_rects > 0);

   SelectClipRect(dri2_drv_surf->private_cdc, NULL);

   for (int i = 0; i < n_rects; i++) {
      const int *rect = &rects[i * 4];
      RECT clip_rc = {rect[0], dri2_drv_surf->base.base.Height - rect[1] - rect[3]};
      clip_rc.right = clip_rc.left + rect[2];
      clip_rc.bottom = clip_rc.top + rect[3];

      IncludeClipRect(dri2_drv_surf->private_cdc, &clip_rc);
   }

   return EGL_TRUE;
}

/**
 * Called via eglSwapBuffers(), drv->API.SwapBuffers().
 */
static EGLBoolean
dri2_minigui_swap_buffers_with_damage(_EGLDriver *drv,
                                 _EGLDisplay *disp,
                                 _EGLSurface *surf,
                                 const EGLint *rects,
                                 EGLint n_rects)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_drv_display *dri2_drv_dpy =
        (struct dri2_egl_drv_display *)dri2_dpy;
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   struct dri2_egl_drv_surface *dri2_drv_surf =
        (struct dri2_egl_drv_surface *)dri2_surf;

   for (int i = 0; i < ARRAY_SIZE(dri2_drv_surf->color_buffers); i++)
      if (dri2_drv_surf->color_buffers[i].age > 0)
         dri2_drv_surf->color_buffers[i].age++;

   /* Make sure we have a back buffer in case we're swapping without ever
    * rendering. */
   if (update_buffers_if_needed(dri2_drv_surf) < 0)
      return _eglError(EGL_BAD_ALLOC, "dri2_swap_buffers");

   dri2_drv_surf->back->age = 1;
   dri2_drv_surf->current = dri2_drv_surf->back;
   dri2_drv_surf->back = NULL;

   if (!dri2_drv_surf->current->memdc) {
      __DRIimage *image;

      if (dri2_dpy->is_different_gpu)
         image = dri2_drv_surf->current->linear_copy;
      else
         image = dri2_drv_surf->current->dri_image;

      dri2_drv_surf->current->memdc =
         create_minigui_buffer(dri2_drv_dpy, dri2_drv_surf, image);

      dri2_drv_surf->current->release = false;
   }

   if (n_rects > 0 && rects) {
      clip_minigui_buffer(dri2_drv_surf, rects, n_rects);
   }

#if 0 // VW
   if (dri2_dpy->is_different_gpu) {
      _EGLContext *ctx = _eglGetCurrentContext();
      struct dri2_egl_context *dri2_ctx = dri2_egl_context(ctx);
      dri2_dpy->image->blitImage(dri2_ctx->dri_context,
                                 dri2_surf->current->linear_copy,
                                 dri2_surf->current->dri_image,
                                 0, 0, dri2_surf->base.Width,
                                 dri2_surf->base.Height,
                                 0, 0, dri2_surf->base.Width,
                                 dri2_surf->base.Height, 0);
   }
#endif

   dri2_flush_drawable_for_swapbuffers(disp, surf);
   if (dri2_dpy->flush)
      dri2_dpy->flush->invalidate(dri2_surf->dri_drawable);

   BitBlt(dri2_drv_surf->current->memdc, 0, 0, 0, 0,
            dri2_drv_surf->private_cdc, 0, 0, 0);

   dri2_minigui_release_buffer(dri2_drv_surf, dri2_drv_surf->current->memdc);

   return EGL_TRUE;
}

static EGLBoolean
dri2_minigui_swap_buffers(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   return dri2_minigui_swap_buffers_with_damage(drv, disp, surf, NULL, 0);
}

static EGLBoolean
dri2_minigui_swrast_swap_buffers(_EGLDriver *drv, _EGLDisplay *disp,
        _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   dri2_dpy->core->swapBuffers(dri2_surf->dri_drawable);

#if 0
   if (dri2_drv_surf->win)
      return dri2_minigui_swap_buffers_with_damage(drv, disp, surf, NULL, 0);
#endif

   return EGL_TRUE;
}

static _EGLImage *
dri2_create_image_khr_pixmap(_EGLDisplay *disp, _EGLContext *ctx,
              EGLClientBuffer buffer, const EGLint *attr_list)
{
#ifndef _MGGAL_DRM
   _eglError(EGL_BAD_PARAMETER,
            "dri2_create_image_khr_pixmap: not supported");
   return NULL;
#else
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_drv_display *dri2_drv_dpy =
        (struct dri2_egl_drv_display *)dri2_dpy;
   struct dri2_egl_image *dri2_img;
   DrmSurfaceInfo info;

   (void) ctx;

   if (!drmGetSurfaceInfo(dri2_drv_dpy->video, (HDC)buffer, &info)
        || info.name == 0) {
      _eglError(EGL_BAD_PARAMETER,
            "dri2_create_image_khr: unsupported pixmap type (bad DRM surface)");
      return NULL;
   }

   dri2_img = malloc(sizeof *dri2_img);
   if (!dri2_img) {
      _eglError(EGL_BAD_ALLOC, "dri2_create_image_khr");
      return EGL_NO_IMAGE_KHR;
   }

   _eglInitImage(&dri2_img->base, disp);

   dri2_img->dri_image =
      dri2_dpy->image->createImageFromName(dri2_dpy->dri_screen,
                  info.width,
                  info.height,
                  info.drm_format,
                  info.name,
                  info.pitch,
                  dri2_img);

   return &dri2_img->base;
#endif
}

static _EGLImage *
dri2_minigui_create_image_khr(_EGLDriver *drv, _EGLDisplay *disp,
           _EGLContext *ctx, EGLenum target,
           EGLClientBuffer buffer, const EGLint *attr_list)
{
   (void) drv;

   switch (target) {
   case EGL_NATIVE_PIXMAP_KHR:
      return dri2_create_image_khr_pixmap(disp, ctx, buffer, attr_list);
   default:
      return dri2_create_image_khr(drv, disp, ctx, target, buffer, attr_list);
   }
}

static const struct dri2_egl_display_vtbl dri2_minigui_swrast_display_vtbl = {
   .authenticate = NULL,
   .create_window_surface = dri2_minigui_create_window_surface,
   .create_pixmap_surface = dri2_minigui_create_pixmap_surface,
   .create_pbuffer_surface = dri2_minigui_create_pbuffer_surface,
   .destroy_surface = dri2_minigui_destroy_surface,
   .create_image = dri2_create_image_khr,
   .swap_buffers = dri2_minigui_swrast_swap_buffers,
   .swap_buffers_with_damage = dri2_fallback_swap_buffers_with_damage,
   .swap_buffers_region = dri2_fallback_swap_buffers_region,
   .post_sub_buffer = dri2_fallback_post_sub_buffer,
   .copy_buffers = dri2_fallback_copy_buffers,
   .query_buffer_age = dri2_fallback_query_buffer_age,
   .create_wayland_buffer_from_image = dri2_fallback_create_wayland_buffer_from_image,
   .get_sync_values = dri2_fallback_get_sync_values,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static const struct dri2_egl_display_vtbl dri2_minigui_drm_display_vtbl = {
   .authenticate = dri2_minigui_authenticate,
   .create_window_surface = dri2_minigui_create_window_surface,
   .create_pixmap_surface = dri2_minigui_create_pixmap_surface,
   .create_pbuffer_surface = dri2_minigui_create_pbuffer_surface,
   .destroy_surface = dri2_minigui_destroy_surface,
   .create_image = dri2_minigui_create_image_khr,
   .swap_buffers = dri2_minigui_swap_buffers,
   .swap_buffers_with_damage = dri2_minigui_swap_buffers_with_damage,
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
   struct dri2_egl_drv_display *dri2_drv_dpy;

   dri2_dpy = calloc(1, sizeof *dri2_drv_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_drv_dpy = (struct dri2_egl_drv_display *)dri2_dpy;
   disp->DriverData = (void *)dri2_dpy;

   dri2_dpy->fd = -1;
   dev = _eglAddDevice(dri2_dpy->fd, true);
   if (!dev) {
      _eglError(EGL_NOT_INITIALIZED, "DRI2: failed to find EGLDevice");
      goto cleanup;
   }

   disp->Device = dev;

   // VW: only ARGB8888, XRGB8888, and RGB565 supported
   BITSET_SET(dri2_drv_dpy->formats, 4);
   BITSET_SET(dri2_drv_dpy->formats, 5);
   BITSET_SET(dri2_drv_dpy->formats, 6);

#if 0
   if (!BITSET_TEST_RANGE(dri2_drv_dpy->formats, 0, EGL_DRI2_MAX_FORMATS)) {
      goto cleanup;
   }
#endif

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

   dri2_drv_dpy->modifiers =
      calloc(ARRAY_SIZE(dri2_mg_visuals), sizeof(*dri2_drv_dpy->modifiers));
   if (!dri2_drv_dpy->modifiers)
      goto cleanup;
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (!u_vector_init(&dri2_drv_dpy->modifiers[i], sizeof(uint64_t), 32))
         goto cleanup;
   }

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

static const __DRIimageLoaderExtension image_loader_extension = {
   .base = { __DRI_IMAGE_LOADER, 1 },

   .getBuffers          = image_get_buffers,
   .flushFrontBuffer    = dri2_minigui_flush_front_buffer,
};

static const __DRIextension *dri2_loader_extensions_old[] = {
   &dri2_loader_extension_old.base,
   &image_lookup_extension.base,
   &background_callable_extension.base,
   NULL,
};

static const __DRIextension *dri2_loader_extensions[] = {
   &dri2_loader_extension.base,
   &image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   &background_callable_extension.base,
   NULL,
};

static const __DRIextension *image_loader_extensions[] = {
   &image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   NULL,
};

static EGLBoolean
dri2_initialize_minigui_dri2(_EGLDriver *drv, _EGLDisplay *disp)
{
   _EGLDevice *dev;
   struct dri2_egl_display *dri2_dpy;
   struct dri2_egl_drv_display *dri2_drv_dpy;

   dri2_dpy = calloc(1, sizeof *dri2_drv_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_drv_dpy = (struct dri2_egl_drv_display *)dri2_dpy;
   disp->DriverData = (void *)dri2_dpy;

   if (disp->PlatformDisplay == NULL) {
      dri2_drv_dpy->video = GetVideoHandle(HDC_SCREEN);
      if (dri2_drv_dpy->video == NULL) {
         _eglError(EGL_BAD_DISPLAY, "DRI2: failed to get MiniGUI video handle");
         goto cleanup;
      }
   } else {
      dri2_drv_dpy->video = disp->PlatformDisplay;
   }

   dri2_drv_dpy->modifiers =
      calloc(ARRAY_SIZE(dri2_mg_visuals), sizeof(*dri2_drv_dpy->modifiers));
   if (!dri2_drv_dpy->modifiers)
      goto cleanup;
   for (int i = 0; i < ARRAY_SIZE(dri2_mg_visuals); i++) {
      if (!u_vector_init(&dri2_drv_dpy->modifiers[i], sizeof(uint64_t), 32))
         goto cleanup;
   }

   dri2_dpy->fd = -1;
#ifdef _MGGAL_DRM
   dri2_dpy->fd = drmGetDeviceFD(dri2_drv_dpy->video);
#endif
   if(dri2_dpy->fd < 0) {
      _eglError(EGL_BAD_DISPLAY, "DRI2: not a MiniGUI DRM engine");
      goto cleanup;
   }
   else {
      dri2_dpy->fd = dup(dri2_dpy->fd);
      if (dri2_dpy->fd < 0) {
         _eglError(EGL_BAD_DISPLAY, "DRI2: failed to duplicate the video fd");
         goto cleanup;
      }
   }

   dev = _eglAddDevice(dri2_dpy->fd, false);
   if (!dev) {
      _eglError(EGL_NOT_INITIALIZED, "DRI2: failed to find EGLDevice");
      goto cleanup;
   }

   disp->Device = dev;

   if (dri2_dpy->dri2_minor >= 1)
      dri2_dpy->loader_extensions = dri2_loader_extensions;
   else
      dri2_dpy->loader_extensions = dri2_loader_extensions_old;

   //dri2_dpy->swap_available = (dri2_dpy->dri2_minor >= 2);
   dri2_dpy->invalidate_available = (dri2_dpy->dri2_minor >= 3);

   dri2_dpy->fd = loader_get_user_preferred_fd(dri2_dpy->fd,
                                               &dri2_dpy->is_different_gpu);
   dev = _eglAddDevice(dri2_dpy->fd, false);
   if (!dev) {
      _eglError(EGL_NOT_INITIALIZED, "DRI2: failed to find EGLDevice");
      goto cleanup;
   }

   disp->Device = dev;

#if 0 // VW
   if (dri2_dpy->is_different_gpu) {
      free(dri2_dpy->device_name);
      dri2_dpy->device_name = loader_get_device_name_for_fd(dri2_dpy->fd);
      if (!dri2_dpy->device_name) {
         _eglError(EGL_BAD_ALLOC, "minigui-egl: failed to get device name "
                                  "for requested GPU");
         goto cleanup;
      }
   }
#endif

   if (dri2_dpy->is_different_gpu) {
      _eglLog(_EGL_WARNING, "DRI2: is_different_gpu TRUE");
   }
   else {
      _eglLog(_EGL_WARNING, "DRI2: is_different_gpu FALSE");
   }

   /* we have to do the check now, because loader_get_user_preferred_fd
    * will return a render-node when the requested gpu is different
    * to the server, but also if the client asks for the same gpu than
    * the server by requesting its pci-id */
   dri2_dpy->is_render_node = drmGetNodeTypeFromFd(dri2_dpy->fd) == DRM_NODE_RENDER;

   dri2_dpy->driver_name = loader_get_driver_for_fd(dri2_dpy->fd);
   if (dri2_dpy->driver_name == NULL) {
      _eglError(EGL_BAD_ALLOC, "DRI2: failed to get driver name");
      goto cleanup;
   }

   /* render nodes cannot use Gem names, and thus do not support
    * the __DRI_DRI2_LOADER extension */
   if (!dri2_dpy->is_render_node) {
      dri2_dpy->loader_extensions = dri2_loader_extensions;
      if (!dri2_load_driver(disp)) {
         _eglError(EGL_BAD_ALLOC, "DRI2: failed to load driver");
         goto cleanup;
      }
   } else {
      dri2_dpy->loader_extensions = image_loader_extensions;
      if (!dri2_load_driver_dri3(disp)) {
         _eglError(EGL_BAD_ALLOC, "DRI3: failed to load driver");
         goto cleanup;
      }
   }

   if (!dri2_create_screen(disp)) {
      _eglError(EGL_BAD_ALLOC, "DRI2: failed to create screen\n");
      goto cleanup;
   }

   if (!dri2_setup_extensions(disp)) {
      _eglError(EGL_BAD_ALLOC, "DRI2: failed to setup externsions\n");
      goto cleanup;
   }

   dri2_setup_screen(disp);

   dri2_minigui_setup_swap_interval(disp);

   if (dri2_dpy->is_different_gpu &&
       (dri2_dpy->image->base.version < 9 ||
        dri2_dpy->image->blitImage == NULL)) {
      _eglLog(_EGL_WARNING, "minigui-egl: Different GPU selected, but the "
                            "Image extension in the driver is not "
                            "compatible. Version 9 or later and blitImage() "
                            "are required");
      goto cleanup;
   }

   // VW: only ARGB8888, XRGB8888, and RGB565 supported
   BITSET_SET(dri2_drv_dpy->formats, 4);
   BITSET_SET(dri2_drv_dpy->formats, 5);
   BITSET_SET(dri2_drv_dpy->formats, 6);

   if (!dri2_minigui_add_configs_for_visuals(drv, disp)) {
      _eglError(EGL_NOT_INITIALIZED, "DRI2: failed to add configs");
      goto cleanup;
   }

   disp->Extensions.EXT_buffer_age = EGL_TRUE;
   disp->Extensions.EXT_swap_buffers_with_damage = EGL_TRUE;

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &dri2_minigui_drm_display_vtbl;

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
   struct dri2_egl_drv_display *dri2_drv_dpy =
        (struct dri2_egl_drv_display *)dri2_dpy;

   for (int i = 0; dri2_drv_dpy->modifiers && i < ARRAY_SIZE(dri2_mg_visuals); i++)
      u_vector_finish(&dri2_drv_dpy->modifiers[i]);
   free(dri2_drv_dpy->modifiers);
}

