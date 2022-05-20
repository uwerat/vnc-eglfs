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
the VNC platform plugin ( https://doc.qt.io/qt-5/qpa.html ) that comes with Qt
should be working just fine.

But for Qt/Quick applications the situation is different as the VNC platform plugin does
not support OpenGL. Rendering with the fallback software renderer
( https://doc.qt.io/QtQuick2DRenderer ) has significant limitations:

- performance aspects ( should be a minor issue for a VNC scenario )
- any native OpenGL fails 
- custom scene graph nodes usually do not have a fallback implementation
- shaders do not work in general

Another problem is that it is only available as platform plugin. So you
can't control the application remotely and locally at the same time.
Actually you always have to restart the application to switch between them.

# How does the VncEglfs solution work

VncEglfs starts VNC servers for QQuickWindows - what kind of corresponds to screens
for EGLFS. Whenever a "frameSwapped" signal happens
( https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html ) the content
of the window is grabbed into a local buffer.

The rest is about mastering the details of the RFB protocol
( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst ) and does not differ
much from what any VNC server implementation has to do.

An obvious problem of this approach is that any update on the screen leads to sending
a complete fullscreen update over the wire. But when enabling JPEG compression the
amount of data to be transferred can usually significantly reduced.

As nowadays many GPUs have on board H.264 encoder it would be interesting to play with it.
Unforatunately I have not seen a VNC viewer that supports that format so far.

# Project status

Beside of the mandatory parts the following aspects of the RFB protocol are implemented:

- Tight/JPEG encoding
- ...

Code has been built for Qt >= 5.12, but it should be possible to also support more
recent versions with adding a few ifdefs.

# How to use

Public APIs have not yet been decided and at the moment you can simply use
the library in a specific mode, where VNC servers are started automatically
when seeing QQuickWindows being exposed.

To enable this you have to add the following line to your application code:

```
#include <VncNamespace.h>

VNC::setup();
```

A viewer connects to a window using the ports starting from 5900.
F.e the second window can be found on 5901. In theory the number of viewers being
connected to the same window is unlimited.

If you want to get rid of the local windows you can run the application with
the "gbm" platform from: https://github.com/uwerat/qpagbm.

For X11 the undocumented "offscreen" platform, that comes with Qt, will do the job as well.
Maybe it also possble to configure a headless mode for eglfs.

# VNC platform integration proxy

If you do not want ( or can't ) touch application code you can load the VNC platform
plugin proxy by using one of these keys: 

    - vnceglfs
    - vncxcb
    - vncwayland

The proxy simply does the initialization above before loading
the real plugin following the "vnc" prefix.

Assuming library and plugin are installed in "/usr/local/vnceglfs":

```
# export QT_DEBUG_PLUGINS=1
export QT_QPA_PLATFORM_PLUGIN_PATH="/usr/local/vnceglfs/plugins"
export LD_LIBRARY_PATH="/usr/local/vnceglfs/lib"

export QT_QPA_PLATFORM=vnceglfs
#export QT_QPA_PLATFORM=vncxcb
#export QT_QPA_PLATFORM=vncwayland
```
