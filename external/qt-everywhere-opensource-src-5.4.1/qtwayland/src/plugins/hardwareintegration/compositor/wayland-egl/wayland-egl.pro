PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)

QT = compositor compositor-private core-private gui-private

OTHER_FILES += wayland-egl.json

SOURCES += \
    main.cpp

include(../../../../hardwareintegration/compositor/wayland-egl/wayland-egl.pri)
