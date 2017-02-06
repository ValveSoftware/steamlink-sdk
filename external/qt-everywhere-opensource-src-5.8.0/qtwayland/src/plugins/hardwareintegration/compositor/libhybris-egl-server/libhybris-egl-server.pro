QT = waylandcompositor waylandcompositor-private core-private gui-private

OTHER_FILES += libhybris-egl-server.json

SOURCES += \
    main.cpp

include($PWD/../../../../../hardwareintegration/compositor/libhybris-egl-server/libhybris-egl-server.pri)

PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)
