# VNC for Qt/Quick on EGLFS

A VNC server for Qt/Quick applications on the EGLFS platform:

- https://en.wikipedia.org/wiki/Virtual_Network_Computing
- https://doc.qt.io/qt-6/qtquick-index.html
- https://doc.qt.io/qt-6/embedded-linux.html

While the motivation of this project is for EGLFS it also works with
X11. I never tried Wayland myself, but it should be possible to
get it running there too.

# Who needs this

Platforms like X11 and Wayland offer VNC support out of the box and when running
one of them this project is probably not for you.

Qt/Widget applications might have some native OpenGL code embedded, but in general
the content of the screen is rendered by the CPU and the approach implemented in
the [Qt VNC platform plugin]( https://doc.qt.io/qt-5/qpa.html ) that comes with Qt
should be working just fine.

But for [Qt/Quick](https://doc.qt.io/qt-6/qtquick-index.html) applications the
situation is different as the [Qt VNC platform plugin]( https://doc.qt.io/qt-5/qpa.html )
has some significant limitations.

# [Qt VNC platform plugin]( https://doc.qt.io/qt-5/qpa.html limitations

The plugin does not support OpenGL at all and rendering is done with
the fallback [software renderer]( https://doc.qt.io/QtQuick2DRenderer ).

- native OpenGL code just fails 

    - custom scene graph nodes usually do not have a fallback implementation
    - shaders do not work in general

- performance aspects

    A minor issue for a VNC scenario, where the network bandwidth is the bottleneck

The implementation of the [RFB]( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst )
is incomplete:

-  No encodings beside [raw]( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#raw-encoding )

   In modern user interfaces you often have smooth transitions ( fading/sliding in/out )
   with full screen updates at 60Hz. Sending uncompressed data with a reasonable update
   rate will choke the network connection.

   Conceptually it would be no big deal to add support of - at least - JPEG compression,
   but making use of the harware accelerated encoding offered by modern GPUs 
   can't be done as efficient as when the image gets rendered on the GPU.

A final problem is this VNC server is only available as platform plugin. So you
can't control the application remotely and locally at the same time.
Actually you always have to restart the application to switch between them.

# The VncEglfs concept

VncEglfs starts VNC servers for QQuickWindows - what kind of corresponds to screens
for EGLFS. Whenever a [frameSwapped](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html )
signal happens the content of the window can be processed.

An obvious problem of this approach is that the server does not know about what
has changed and always sends fullscreen updates over the wire. This makes using compressed formats
like JPEG or H.264 more or less mandatory.
As nowadays many GPUs offer hardware accelerated encoding it should be possible
to do the encoding on the GPU before downloading the frame.

The rest is about mastering the details of the [RFB protocol]
( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst ) and does not differ
much from what any VNC server implementation has to do.

# Project status

- Implemented:

    - All mandatory parts of the RFB protocol ( similar to what is supported
      by the Qt VNC plugin + mouse wheel and more keys ).

    - [Tight/JPEG]( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#tight-encoding )

      Using the encoder from [Qt's image I/O system]( https://doc.qt.io/qt-6/qtimageformats-index.html),
      usually a wrapper for: [libjpeg-turbo]( https://libjpeg-turbo.org/ )

      First attempts have been made with [hardware accelerated encoding]( https://intel.github.io/libva/group__api__enc__jpeg.html )
      with only limited "success" using the old driver ( export LIBVA_DRIVER_NAME=i965 )

      - colors are wrong
      - lines are shifted, when setting certain values for the quality
      - transferring the image to a VASurface includes down/up-loading from/to the GPU

      Encoding seems to be more than twice as fast for an image of 600x600 pixels
      ( including the extra upload ) compared to libjpeg-turbo

- Planned

    - Authentification

    - [H.264 ] ( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#open-h-264-encoding )

      Looks like support for H.264 has been [added recently]( https://github.com/TigerVNC/tigervnc/pull/1194 )
      to the [TigerVNC]( https://github.com/TigerVNC ) viewer.

    - Encoding the images on the GPU

      If you are familiar with [VA_API]( https://en.wikipedia.org/wiki/Video_Acceleration_API ) and want to
      help: let me know.
      
Code has been built for Qt >= 5.12, but it should be possible to support older
releases with adding some ifdefs.

# How to use

Public APIs have not yet been decided and at the moment you can only use
the library in a specific mode, where VNC servers are started automatically
when seeing QQuickWindows being exposed.

A viewer connects to a window using the ports starting from 5900.
F.e the second window can be found on 5901. In theory the number of viewers being
connected to the same window is unlimited.

You can enable VNC support by doing the initilization ithe application code or by
running the application with the VNC platform proxy plugin.
 
### Application code

Add the following line somewhere in the code:

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

export QT_QPA_PLATFORM=vnceglfs # vncxcb, vncwayland
```
