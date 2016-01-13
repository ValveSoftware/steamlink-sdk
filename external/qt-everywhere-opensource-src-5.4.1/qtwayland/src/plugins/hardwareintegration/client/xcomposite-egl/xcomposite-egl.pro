PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)

QT += waylandclient-private

include(../../../../hardwareintegration/client/xcomposite-egl/xcomposite-egl.pri)

OTHER_FILES += xcomposite-egl.json

SOURCES += \
    main.cpp

