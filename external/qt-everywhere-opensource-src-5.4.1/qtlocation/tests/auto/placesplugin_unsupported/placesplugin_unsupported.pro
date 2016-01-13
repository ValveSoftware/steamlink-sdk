TARGET = qtgeoservices_placesplugin_unsupported
QT += location

PLUGIN_TYPE = geoservices
PLUGIN_CLASS_NAME = UnsupportedPlacesGeoServicePlugin
PLUGIN_EXTENDS = -
load(qt_plugin)

HEADERS += qgeoserviceproviderplugin_test.h

SOURCES += qgeoserviceproviderplugin_test.cpp

OTHER_FILES += \
    placesplugin.json
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
