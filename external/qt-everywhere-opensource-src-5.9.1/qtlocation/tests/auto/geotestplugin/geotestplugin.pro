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
           qgeotiledmap_test.h \
           qgeotilefetcher_test.h

SOURCES += qgeoserviceproviderplugin_test.cpp \
           qgeotiledmap_test.cpp

OTHER_FILES += \
    geotestplugin.json \
    place_data.json
RESOURCES += testdata.qrc
