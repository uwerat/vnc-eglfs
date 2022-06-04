# VNC for Qt/Quick on EGLFS

This project implements a [VNC](https://en.wikipedia.org/wiki/Virtual_Network_Computing)
server for [Qt/Quick](https://doc.qt.io/qt-6/qtquick-index.html) windows.

The basic idea of this server is to grab each frame from the GPU and to forward it
to the [RFB protocol]( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst ).<br>
This implementation is not affected by [limitions]( https://doc.qt.io/QtQuick2DRenderer/qtquick2drenderer-limitations.html )
of the [software renderer]( https://doc.qt.io/QtQuick2DRenderer ) and allows having native OpenGL code
in application code ( custom scene graph nodes ).

As the [Qt/Quick](https://doc.qt.io/qt-6/qtquick-index.html) technology is for
"fluid and dynamic user interfaces" the VNC server has to be able to forward
full updates ( f.e. fade in/out ) with an acceptable rate to the viewer, what
makes [image compression]( https://en.wikipedia.org/wiki/Image_compression) more or less mandatory.
Fortunately modern [GPUs](https://en.wikipedia.org/wiki/Graphics_processing_unit) usually offer
[encoding]( https://en.wikipedia.org/wiki/Graphics_processing_unit#GPU_accelerated_video_decoding_and_encoding)
for [JPEG]( https://en.wikipedia.org/wiki/JPEG ) and [H.264]( https://en.wikipedia.org/wiki/Advanced_Video_Coding ).
Encoding on the GPU also avoids the expensive calls of
[glReadPixels](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glReadPixels.xhtml) for each frame.

Obvious limitations:

- no partial updates

    A VNC server always transfers complete frames without trying to optimize for partial updates.
    Despite the fact that there is no ( at least I do not know one ) unexpensive way to identify
    the update regions it would not help much for situations like swiping, fading in/out etc.
    Of course this leads to more traffic than necessary for other situations like pressing a button.
    Using a compression like [H.264]( https://en.wikipedia.org/wiki/Advanced_Video_Coding ),
    that is using differences between frames, might be the best solution.

- windows instead of screens

    Usually VNC is used to mirror a complete screen/desktop with several windows
    from different applications. However the "recommended plugin for modern Embedded Linux devices"
    [EGLFS]( https://doc.qt.io/qt-6/embedded-linux.html ) only supports top level windows
    in fullscreen mode - what kind of equals windows and screens.
    For other platforms - like xcb or wayland - good solutions for mirroring a desktop are
    available.

# Project Status

The current status of the implementation was tested for Qt >= 5.12 with
remote connections to an application running on EGLFS and XCB.

Numbers depend on the capabilities of the devices and the size/content of the window,
but on my test system ( Intel i7-8559U ) I was able to display a window with 800x800 pixels
with >= 20fps remotely, when JPEG compression is enabled.
The number was found by running <em>xtigervncviewer -Log '*:stderr:100'</em> as viewer.

These features are implemented:

- mandatory parts of the RFB protocol

    This similar to what is supported by the Qt VNC plugin ( + mouse wheel, additional key codes )

- [Tight/JPEG]( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#tight-encoding )

    Using the encoder from [Qt's image I/O system]( https://doc.qt.io/qt-6/qtimageformats-index.html),
    usually a wrapper for: [libjpeg-turbo]( https://libjpeg-turbo.org/ )

The following important parts are missing:

- [Authentification]( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#security-types )

- [H.264 ]( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#open-h-264-encoding )

    Looks like support for H.264 has been [added recently]( https://github.com/TigerVNC/tigervnc/pull/1194 )
    to the [TigerVNC]( https://github.com/TigerVNC ) viewer.

- hardware video acceleration: [VA_API]( https://en.wikipedia.org/wiki/Video_Acceleration_API )

    Without compressing the frames early on the GPU the performance of the pipeline suffers
    from expensive glReadPixels calls and what is needed to compress the image on the CPU.<br>
    If you are familiar with [libva]( http://intel.github.io/libva/group__api__core.html)
    and want to help: let me know.

# How to use

There are 2 way how to enable VNC support for an applation:

- [C++ API]( https://github.com/uwerat/vnc-eglfs/blob/main/src/VncNamespace.h )

    The C++ API allows to configure, start and stop a VNC servers individually.

- [VNC platform integration proxy]( https://github.com/uwerat/vnc-eglfs/blob/main/platformproxy/VncProxyPlugin.cpp )

    The VNC platform proxy allows to start VNC servers for applications that can't be modified
    or recompiled.

Both solutions are affected by the following environment variables:

- QVNC_GL_PORT

   The first unused port >= $QVNC_GL_PORT will be used when starting a server

- QVNC_GLTIMER_INTERVAL

   each server is periodically checking if a new frame is available
   and the viewer is ready to accept it. Increasing the interval might
   decrease the number of updates being displayed in the viewer.

### Application code

The most simple way to enable VNC support is to add the following line somewhere:

```
#include <VncNamespace.h>

VNC::setup();
```

If you want to get rid of the local windows you have several options:

- using the [gbm platform plugin](https://github.com/uwerat/qpagbm)
- using the undocumented "offscreen" platform, that comes with Qt ( X11 only )
- reconfiguring a [headless](https://doc.qt.io/qt-5/embedded-linux.html#advanced-eglfs-kms-features) mode ( EGLFS only  )

### VNC platform integration proxy

If you do not want ( or can't ) touch application code you can load the VNC platform
plugin proxy by using one of these: [keys](https://github.com/uwerat/vnc-eglfs/blob/main/platformproxy/metadata.json).
The [proxy](https://github.com/uwerat/vnc-eglfs/blob/main/platformproxy/VncProxyPlugin.cpp)
simply does the initialization above before loading the real plugin following the "vnc" prefix.

Assuming library and plugin are installed in "/usr/local/vnceglfs":

```
# export QT_DEBUG_PLUGINS=1
export QT_QPA_PLATFORM_PLUGIN_PATH="/usr/local/vnceglfs/plugins"
export LD_LIBRARY_PATH="/usr/local/vnceglfs/lib"

export QT_QPA_PLATFORM=vnceglfs # vncxcb, vncwayland, vncoffscreen, vncgbm
```
