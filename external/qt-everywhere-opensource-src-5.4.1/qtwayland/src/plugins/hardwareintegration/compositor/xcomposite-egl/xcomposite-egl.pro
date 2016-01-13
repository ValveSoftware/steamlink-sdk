PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)

QT += compositor compositor-private core-private gui-private

OTHER_FILES += xcomposite-egl.json

SOURCES += \
    main.cpp

include(../../../../hardwareintegration/compositor/xcomposite-egl/xcomposite-egl.pri)
