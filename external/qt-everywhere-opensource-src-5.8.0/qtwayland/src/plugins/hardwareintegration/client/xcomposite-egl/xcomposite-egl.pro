# We have a bunch of C code with casts, so we can't have this option
QMAKE_CXXFLAGS_WARN_ON -= -Wcast-qual

QT += waylandclient-private

include(../../../../hardwareintegration/client/xcomposite-egl/xcomposite-egl.pri)

OTHER_FILES += xcomposite-egl.json

SOURCES += \
    main.cpp

PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)
