QT += waylandclient-private

include(../../../hardwareintegration/client/xcomposite-glx/xcomposite-glx.pri)

OTHER_FILES += qwayland-xcomposite-glx.json

SOURCES += \
    main.cpp

HEADERS += \
    qwaylandxcompositeglxplatformintegration.h

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QWaylandXCompositeGlxPlatformIntegrationPlugin
load(qt_plugin)
