QT += waylandclient-private

include(../../../../hardwareintegration/client/wayland-egl/wayland-egl.pri)

OTHER_FILES += \
    wayland-egl.json

SOURCES += main.cpp

PLUGIN_TYPE = wayland-graphics-integration-client
PLUGIN_CLASS_NAME = QWaylandEglClientBufferPlugin
load(qt_plugin)
