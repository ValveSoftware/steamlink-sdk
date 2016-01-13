TARGET  = qtiff

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QTiffPlugin
load(qt_plugin)

HEADERS += qtiffhandler_p.h
SOURCES += main.cpp qtiffhandler.cpp
wince*: SOURCES += qfunctions_wince.cpp
OTHER_FILES += tiff.json

config_libtiff {
    unix|mingw: LIBS += -ltiff
    else:win32: LIBS += libtiff.lib
} else {
    include($$PWD/../../../3rdparty/libtiff.pri)
}
