QT += waylandclient-private

include(../../../hardwareintegration/client/xcomposite-egl/xcomposite-egl.pri)

OTHER_FILES += qwayland-xcomposite-egl.json

SOURCES += \
    main.cpp

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QWaylandXCompositeEglPlatformIntegrationPlugin
load(qt_plugin)
