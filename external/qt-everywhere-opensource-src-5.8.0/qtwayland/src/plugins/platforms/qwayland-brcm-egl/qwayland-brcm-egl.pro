QT += waylandclient-private

include(../../../hardwareintegration/client/brcm-egl/brcm-egl.pri)

OTHER_FILES += \
    qwayland-brcm-egl.json

SOURCES += main.cpp

PLUGIN_TYPE = platforms
load(qt_plugin)
