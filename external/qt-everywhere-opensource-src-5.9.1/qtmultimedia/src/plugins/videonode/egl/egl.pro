TARGET = eglvideonode

QT += multimedia-private qtmultimediaquicktools-private
CONFIG += egl

HEADERS += \
    qsgvideonode_egl.h

SOURCES += \
    qsgvideonode_egl.cpp

OTHER_FILES += \
    egl.json

PLUGIN_TYPE = video/videonode
PLUGIN_EXTENDS = quick
PLUGIN_CLASS_NAME = QSGVideoNodeFactory_EGL
load(qt_plugin)
