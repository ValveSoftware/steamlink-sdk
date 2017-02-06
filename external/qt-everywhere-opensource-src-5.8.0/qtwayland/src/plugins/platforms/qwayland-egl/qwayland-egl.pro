QT += waylandclient-private

include(../../../hardwareintegration/client/wayland-egl/wayland-egl.pri)

OTHER_FILES += \
    qwayland-egl.json

SOURCES += main.cpp

PLUGIN_TYPE = platforms
load(qt_plugin)
