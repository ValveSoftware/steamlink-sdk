QT += waylandcompositor waylandcompositor-private core-private gui-private

OTHER_FILES += xcomposite-glx.json

SOURCES += \
    main.cpp

include(../../../../hardwareintegration/compositor/xcomposite-glx/xcomposite-glx.pri)

PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)
