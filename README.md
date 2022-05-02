# VNC for Qt/Quick on EGLFS

This project offers a VNC server for Qt/Quick applications on the EGLFS platform:

- https://en.wikipedia.org/wiki/Virtual_Network_Computing
- https://doc.qt.io/qt-6/qtquick-index.html
- https://doc.qt.io/qt-6/embedded-linux.html

It is supposed to work with all Qt versions >= 5.0

# Who needs this

Platforms like X11 and Wayland offer VNC support out of the box and when running
one of them this project is probably not for you.

Qt/Widget applications might have some native OpenGL code embedded, but in general
the content of the screen is rendered by the CPU and the approach implemented in
the VNC platform plugin ( https://doc.qt.io/qt-5/qpa.html ) that comes with Qt
should be working just fine.

But for Qt/Quick applications the situation is different as the VNC platform plugin does
not support OpenGL. Rendering with the fallback software renderer
( https://doc.qt.io/QtQuick2DRenderer ) has some problems:

- performance aspects ( should be a minor issue for a VNC scenario )
- custom scene graph nodes usually do not have a fallback implementation
- shaders do not work in general

Another limitation is that it is only available as platform plugin. So you
can't control the application remotely and locally at the same time.
Actually you always have to restart the application to switch between them.

# How does the VncEglfs solution work

VncEglfs starts a VNC servers for QQuickWindows - what kind of corresponds to screens
for EGLFS. Whenever a "frameSwapped" signal happens
( https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html ) the content
of the window is grabbed into a local buffer.

The rest is about mastering the details of the RFB protocol
( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst ) and does not differ
much from what any VNC server implementation has to do.

An obvious problem of this approach is that any update on the screen leads to sending
a complete fullscreen update over the wire. Considering that the Qt/Quick graphic stack is
designed to support user interfaces with smooth transitions a low bandwidth connection will
not match the requirements of the update rate.

Using Hardware accelerated H.264 encoding on the GPU might be a logical solution to this problem.

# Project status

At the moment only the mandatory parts of the RFB protocol are implemented:

- Framebuffer is always sent as "Raw" data.
- No authentication
- Only V3.3
- X11 ( xcb platform plugin ) is also supported - mainly for development purposes
- ...

# How to use

Public APIs have not yet been decided and at the moment you can simply use
the library in a specific mode, where VNC servers are started automatically
when seeing QQuickWindows being exposed.

To enable this you have to add the following line to your application code:

```
#include <VncNamespace.h>

VNC::setup();
```

A viewer can connect to a window using ports starting from 5900.
F.e the second window will then be on 5901.
