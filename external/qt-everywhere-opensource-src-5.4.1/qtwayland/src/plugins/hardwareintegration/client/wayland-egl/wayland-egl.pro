PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)

QT += waylandclient-private

include(../../../../hardwareintegration/client/wayland-egl/wayland-egl.pri)

OTHER_FILES += \
    wayland-egl.json

SOURCES += main.cpp

