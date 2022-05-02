# VNC for Qt/Quick on EGLFS

This project offers a VNC server ( https://en.wikipedia.org/wiki/Virtual_Network_Computing )
for Qt/Quick applications on the EGLFS platform:

- https://en.wikipedia.org/wiki/Virtual_Network_Computing
- https://doc.qt.io/qt-6/qtquick-index.html
- https://doc.qt.io/qt-6/embedded-linux.html

# Who needs this ?

X11 and Wayland offer VNC support out of the box and when running one of these
platforms this project is not for you.

Qt/Widget applications might have some native OpenGL code embedded, but in general
the content of the screen is rendered by the CPU and the approach implemented in
the VNC platform plugin ( https://doc.qt.io/qt-5/qpa.html ) should be working
just fine.

But for Qt/Quick applications the situation is different as the VNC platform plugin does
not support OpenGL and rendering falls back on the software renderer
( https://doc.qt.io/QtQuick2DRenderer ).

The performace aspect should be a minor issue for the VNC scenario but you often do not have
a fallback implementation in the software rendering - usually, when having custom nodes
or nodes that need shaders.

Another limitation of the solution offered by the Qt company is, that it is only available
as platform plugin and you can't run it together with the native platform. This makes it
impossible to run a VNC client together with the user interface on the embedded device and
you have to restart the application to switch between local/remote.

# How does the VNCEglfs solution work

VNCEglfs starts a VNC server for each QQuickWindow - what is more or less the same
as a screen for platform like EGLFS. Whenever a "frameSwapped" signal happenes
( see https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html ) the content
of the window is grabbed into a local buffer.

The rest is about mastering the details of the Rfb protocol
( https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst ) and does not differ
much from what any VNC server implementation has to do.

An obvious problem of this approach is, that any update on the screen leads to sending
a complete framebuffer over the wire. Considering that the Qt/Quick graphic stack is
designed to support smooth transitions a low bandwidth connection will run into problems.

Hardware accelerated H.264 encoding might be a logical solution to this problem.

# Project status

At the moment only the mandatory parts of the RFB protocol are implemented:

- Framebuffer is always sent as "Raw" data.
- No authentication
- Only V3.3
