TEMPLATE = lib

CONFIG += plugin
CONFIG += silent
CONFIG += hide_symbols

CONFIG += hide_symbols
CONFIG += no_private_qt_headers_warning

QT += gui gui-private

MOC_DIR=moc
OBJECTS_DIR=obj

TARGET = $$qtLibraryTarget(vncproxy)
DESTDIR = plugins/platforms

DEFINES += VNC_DLL

PROJECT_ROOT = $$clean_path( $$PWD/../src )

INCLUDEPATH *= $${PROJECT_ROOT}
DEPENDPATH *= $${PROJECT_ROOT}

LIBS *= -L$${PROJECT_ROOT}/lib -lvncgl

SOURCES += \
    VncProxyPlugin.cpp

OTHER_FILES += metadata.json
