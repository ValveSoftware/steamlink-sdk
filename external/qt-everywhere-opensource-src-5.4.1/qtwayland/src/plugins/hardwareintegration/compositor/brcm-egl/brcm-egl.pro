PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)

QT = compositor compositor-private core-private gui-private

OTHER_FILES += brcm-egl.json

LIBS += -lEGL

SOURCES += \
    main.cpp

include(../../../../hardwareintegration/compositor/brcm-egl/brcm-egl.pri)
