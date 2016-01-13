PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)

QT += waylandclient-private

include(../../../../hardwareintegration/client/drm-egl-server/drm-egl-server.pri)

LIBS += -lEGL

OTHER_FILES += \
    drm-egl-server.json

SOURCES += main.cpp

