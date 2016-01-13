TARGET = qtgeoservices_qmltestplugin
QT += location-private positioning-private testlib

PLUGIN_TYPE = geoservices
PLUGIN_CLASS_NAME = TestGeoServicePlugin
PLUGIN_EXTENDS = -
load(qt_plugin)

HEADERS += qgeocodingmanagerengine_test.h \
           qgeoserviceproviderplugin_test.h \
           qgeoroutingmanagerengine_test.h \
           qplacemanagerengine_test.h \
           qgeotiledmappingmanagerengine_test.h \
           qgeotiledmapdata_test.h \
           qgeotilefetcher_test.h

SOURCES += qgeoserviceproviderplugin_test.cpp

OTHER_FILES += \
    geotestplugin.json \
    place_data.json
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
