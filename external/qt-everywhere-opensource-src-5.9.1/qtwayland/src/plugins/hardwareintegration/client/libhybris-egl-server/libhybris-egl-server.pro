QT += waylandclient-private

include(../../../../hardwareintegration/client/libhybris-egl-server/libhybris-egl-server.pri)

OTHER_FILES += \
    libhybris-egl-server.json

SOURCES += main.cpp

PLUGIN_TYPE = wayland-graphics-integration-client
PLUGIN_CLASS_NAME = LibHybrisEglServerBufferPlugin
load(qt_plugin)
