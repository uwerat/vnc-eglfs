TEMPLATE = lib

CONFIG += plugin
CONFIG += silent
CONFIG += warn_on

CONFIG += hide_symbols
CONFIG += no_private_qt_headers_warning

CONFIG += strict_c++
CONFIG += c++11

QT += gui gui-private

MOC_DIR=moc
OBJECTS_DIR=obj

TARGET = $$qtLibraryTarget(vncproxy)
DESTDIR = plugins/platforms

PROJECT_ROOT = $$clean_path( $$PWD/../src )

INCLUDEPATH *= $${PROJECT_ROOT}
DEPENDPATH *= $${PROJECT_ROOT}

LIBS *= -L$${PROJECT_ROOT}/lib -lvncgl

SOURCES += \
    VncProxyPlugin.cpp

OTHER_FILES += metadata.json

INSTALL_ROOT=/usr/local/vnceglfs
# INSTALL_ROOT=$$[QT_INSTALL_PREFIX]

target.path = $${INSTALL_ROOT}/plugins/platforms
INSTALLS += target
