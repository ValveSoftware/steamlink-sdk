PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)

QT += waylandclient-private

include(../../../../hardwareintegration/client/libhybris-egl-server/libhybris-egl-server.pri)

LIBS += -lEGL

OTHER_FILES += \
    libhybris-egl-server.json

SOURCES += main.cpp

