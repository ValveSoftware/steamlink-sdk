QT += waylandclient-private

include(../../../hardwareintegration/client/wayland-egl/wayland-egl.pri)

OTHER_FILES += \
    qwayland-egl.json

SOURCES += main.cpp

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QWaylandEglPlatformIntegrationPlugin
load(qt_plugin)
