TEMPLATE = app
CONFIG += testcase
TARGET = tst_qgeomapcontroller

HEADERS += ../geotestplugin/qgeoserviceproviderplugin_test.h \
           ../geotestplugin/qgeotiledmapdata_test.h \
           ../geotestplugin/qgeotiledmappingmanagerengine_test.h \
           ../geotestplugin/qplacemanagerengine_test.h \
           ../geotestplugin/qgeotilefetcher_test.h \
           ../geotestplugin/qgeoroutingmanagerengine_test.h \
           ../geotestplugin/qgeocodingmanagerengine_test.h

SOURCES += tst_qgeomapcontroller.cpp
SOURCES += ../geotestplugin/qgeoserviceproviderplugin_test.cpp

QT += location-private positioning-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
