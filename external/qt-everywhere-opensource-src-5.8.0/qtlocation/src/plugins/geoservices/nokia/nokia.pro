TARGET = qtgeoservices_nokia

QT += location-private positioning-private network

HEADERS += \
            qgeocodereply_nokia.h \
            qgeocodejsonparser.h \
            qgeocodingmanagerengine_nokia.h \
            qgeotiledmappingmanagerengine_nokia.h \
            qgeotilefetcher_nokia.h \
            qgeomapreply_nokia.h \
            qgeoroutereply_nokia.h \
            qgeoroutexmlparser.h \
            qgeoroutingmanagerengine_nokia.h \
            qgeoserviceproviderplugin_nokia.h \
            marclanguagecodes.h \
            qgeonetworkaccessmanager.h \
            qgeointrinsicnetworkaccessmanager.h \
            qgeouriprovider.h \
            uri_constants.h \
            qgeoerror_messages.h \
            qgeomapversion.h \
            qgeotiledmap_nokia.h \
            qgeofiletilecachenokia.h


SOURCES += \
            qgeocodereply_nokia.cpp \
            qgeocodejsonparser.cpp \
            qgeocodingmanagerengine_nokia.cpp \
            qgeotiledmappingmanagerengine_nokia.cpp \
            qgeotilefetcher_nokia.cpp \
            qgeomapreply_nokia.cpp \
            qgeoroutereply_nokia.cpp \
            qgeoroutexmlparser.cpp \
            qgeoroutingmanagerengine_nokia.cpp \
            qgeoserviceproviderplugin_nokia.cpp \
            qgeointrinsicnetworkaccessmanager.cpp \
            qgeouriprovider.cpp \
            uri_constants.cpp \
            qgeoerror_messages.cpp \
            qgeomapversion.cpp \
            qgeotiledmap_nokia.cpp \
            qgeofiletilecachenokia.cpp

include(placesv2/placesv2.pri)

RESOURCES += resource.qrc

INCLUDEPATH += ../../../location/maps

OTHER_FILES += \
    nokia_plugin.json

PLUGIN_TYPE = geoservices
PLUGIN_CLASS_NAME = QGeoServiceProviderFactoryNokia
load(qt_plugin)
