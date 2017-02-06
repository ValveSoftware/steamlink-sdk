QT += waylandclient-private

include(../../../hardwareintegration/client/xcomposite-egl/xcomposite-egl.pri)

OTHER_FILES += qwayland-xcomposite-egl.json

SOURCES += \
    main.cpp

PLUGIN_TYPE = platforms
load(qt_plugin)
