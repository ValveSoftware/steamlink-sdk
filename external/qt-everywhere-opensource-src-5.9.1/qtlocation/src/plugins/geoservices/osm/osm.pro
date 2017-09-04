TARGET = qtgeoservices_osm

QT += location-private positioning-private network concurrent

HEADERS += \
    qgeoserviceproviderpluginosm.h \
    qgeotiledmappingmanagerengineosm.h \
    qgeotilefetcherosm.h \
    qgeomapreplyosm.h \
    qgeocodingmanagerengineosm.h \
    qgeocodereplyosm.h \
    qgeoroutingmanagerengineosm.h \
    qgeoroutereplyosm.h \
    qplacemanagerengineosm.h \
    qplacesearchreplyosm.h \
    qplacecategoriesreplyosm.h \
    qgeotiledmaposm.h \
    qgeofiletilecacheosm.h \
    qgeotileproviderosm.h

SOURCES += \
    qgeoserviceproviderpluginosm.cpp \
    qgeotiledmappingmanagerengineosm.cpp \
    qgeotilefetcherosm.cpp \
    qgeomapreplyosm.cpp \
    qgeocodingmanagerengineosm.cpp \
    qgeocodereplyosm.cpp \
    qgeoroutingmanagerengineosm.cpp \
    qgeoroutereplyosm.cpp \
    qplacemanagerengineosm.cpp \
    qplacesearchreplyosm.cpp \
    qplacecategoriesreplyosm.cpp \
    qgeotiledmaposm.cpp \
    qgeofiletilecacheosm.cpp \
    qgeotileproviderosm.cpp


OTHER_FILES += \
    osm_plugin.json

PLUGIN_TYPE = geoservices
PLUGIN_CLASS_NAME = QGeoServiceProviderFactoryOsm
load(qt_plugin)
