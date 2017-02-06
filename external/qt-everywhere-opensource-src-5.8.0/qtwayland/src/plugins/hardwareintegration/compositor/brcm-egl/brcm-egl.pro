QT = waylandcompositor waylandcompositor-private core-private gui-private

OTHER_FILES += brcm-egl.json

SOURCES += \
    main.cpp

include(../../../../hardwareintegration/compositor/brcm-egl/brcm-egl.pri)

PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)
