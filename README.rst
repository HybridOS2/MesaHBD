`Mesa <https://mesa3d.org>`_ - The 3D Graphics Library
======================================================

`MesaHBD` is the Mesa derivative for HybridOS.

Source
------

This reposistory lives at https://github.com/HybridOS2/MesaHBD.

The original Mesa lives at https://gitlab.freedesktop.org/mesa/mesa.

Build & install
---------------

You can find more information in our documentation (`docs/install.html
<https://mesa3d.org/install.html>`_), but the recommended way is to use
Meson (`docs/meson.html <https://mesa3d.org/meson.html>`_):

.. code-block:: sh

  $ mkdir build
  $ cd build
  $ meson ..
  $ sudo ninja install

You must specify the library suffix `hbd` for HybridOS, for example,

.. code-block:: sh

  $ mkdir build
  $ cd build
  $ meson .. -Dgallium-drivers= -Ddri-drivers=swrast -Dvulkan-drivers= -Dllvm=false -Dglx=disabled -Degl-lib-suffix=hbd -Dgles-lib-suffix=hbd -Dgl-lib-suffix=hbd
  $ sudo ninja install

Support
-------

Many Mesa devs hang on IRC; if you're not sure which channel is
appropriate, you should ask your question on `Freenode's #dri-devel
<irc://chat.freenode.net#dri-devel>`_, someone will redirect you if
necessary.
Remember that not everyone is in the same timezone as you, so it might
take a while before someone qualified sees your question.
To figure out who you're talking to, or which nick to ping for your
question, check out `Who's Who on IRC
<https://dri.freedesktop.org/wiki/WhosWho/>`_.

The next best option is to ask your question in an email to the
mailing lists: `mesa-dev\@lists.freedesktop.org
<https://lists.freedesktop.org/mailman/listinfo/mesa-dev>`_


Bug reports
-----------

If you think something isn't working properly, please file a bug report
(`docs/bugs.html <https://mesa3d.org/bugs.html>`_).


Contributing
------------

Contributions are welcome, and step-by-step instructions can be found in our
documentation (`docs/submittingpatches.html
<https://mesa3d.org/submittingpatches.html>`_).

Note that Mesa uses email mailing-lists for patches submission, review and
discussions.
