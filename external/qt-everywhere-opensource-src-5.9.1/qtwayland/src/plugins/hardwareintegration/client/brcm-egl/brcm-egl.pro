QT += waylandclient-private

include(../../../../hardwareintegration/client/brcm-egl/brcm-egl.pri)

OTHER_FILES += \
    brcm-egl.json

SOURCES += main.cpp

PLUGIN_TYPE = wayland-graphics-integration-client
PLUGIN_CLASS_NAME = QWaylandBrcmEglClientBufferPlugin
load(qt_plugin)
