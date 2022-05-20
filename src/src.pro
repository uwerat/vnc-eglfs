TEMPLATE = lib
TARGET   = qvnceglfs

DEFINES += VNC_DLL VNC_MAKEDLL

QT += gui gui-private network

CONFIG += hide_symbols
CONFIG += silent
CONFIG += no_private_qt_headers_warning

# CONFIG += videoacceleration

MOC_DIR=moc
OBJECTS_DIR=obj

TARGET = $$qtLibraryTarget(vncgl)
DESTDIR = lib

HEADERS += \
    RfbSocket.h \
    RfbPixelStreamer.h \
    RfbEncoder.h \
    RfbInputEventHandler.h \
    VncServer.h \
    VncClient.h \
    VncNamespace.h \

SOURCES += \
    RfbSocket.cpp \
    RfbPixelStreamer.cpp \
    RfbEncoder.cpp \
    RfbInputEventHandler.cpp \
    VncServer.cpp \
    VncClient.cpp \
    VncNamespace.cpp \

videoacceleration {

    HEADERS += \
        va/VncJpeg.h \
        va/VncVaRenderer.h \
        va/VncVaEncoder.h

    SOURCES += \
        va/VncJpeg.cpp \
        va/VncVaRenderer.cpp \
        va/VncVaEncoder.cpp

    DEFINES += VNC_VA_ENCODER

    CONFIG += link_pkgconfig
    PKGCONFIG += libva-drm libva
}
