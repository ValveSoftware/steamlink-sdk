TARGET = qtposition_geoclue
QT = core positioning

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryGeoclue
load(qt_plugin)

HEADERS += \
    qgeopositioninfosource_geocluemaster_p.h \
    qgeopositioninfosourcefactory_geoclue.h \
    qgeocluemaster.h

SOURCES += \
    qgeopositioninfosource_geocluemaster.cpp \
    qgeopositioninfosourcefactory_geoclue.cpp \
    qgeocluemaster.cpp

qtHaveModule(dbus):config_geoclue-satellite {
    DEFINES += HAS_SATELLITE

    QT *= dbus

    HEADERS += qgeosatelliteinfosource_geocluemaster.h
    SOURCES += qgeosatelliteinfosource_geocluemaster.cpp
}

INCLUDEPATH += $$QT.location.includes

CONFIG += link_pkgconfig
PKGCONFIG += geoclue

OTHER_FILES += \
    plugin.json \
    plugin-satellite.json
