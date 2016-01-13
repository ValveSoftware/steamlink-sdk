TARGET = qtgeoservices_routingplugin
QT += location

PLUGIN_TYPE = geoservices
PLUGIN_CLASS_NAME = RoutingTestGeoServicePlugin
PLUGIN_EXTENDS = -
load(qt_plugin)

HEADERS += qgeoroutingmanagerengine_test.h \
           qgeoserviceproviderplugin_test.h

SOURCES += qgeoserviceproviderplugin_test.cpp

OTHER_FILES += \
    routing_plugin.json
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
