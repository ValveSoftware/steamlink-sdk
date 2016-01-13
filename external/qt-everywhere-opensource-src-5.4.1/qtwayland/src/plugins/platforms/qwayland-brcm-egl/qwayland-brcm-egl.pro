PLUGIN_TYPE = platforms
load(qt_plugin)

QT += waylandclient-private

include(../../../hardwareintegration/client/brcm-egl/brcm-egl.pri)

LIBS += -lEGL

OTHER_FILES += \
    qwayland-brcm-egl.json

SOURCES += main.cpp

