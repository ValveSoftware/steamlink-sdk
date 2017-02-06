# We have a bunch of C code with casts, so we can't have this option
QMAKE_CXXFLAGS_WARN_ON -= -Wcast-qual

QT += waylandclient-private

include(../../../../hardwareintegration/client/drm-egl-server/drm-egl-server.pri)

OTHER_FILES += \
    drm-egl-server.json

SOURCES += main.cpp

PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)
