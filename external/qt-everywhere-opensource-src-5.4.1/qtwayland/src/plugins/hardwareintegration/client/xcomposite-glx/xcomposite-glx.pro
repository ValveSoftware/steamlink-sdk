PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)

QT += waylandclient-private

include(../../../../hardwareintegration/client/xcomposite-glx/xcomposite-glx.pri)

OTHER_FILES += xcomposite-glx.json

SOURCES += \
    main.cpp

