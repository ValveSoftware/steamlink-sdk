TARGET = qtposition_geoclue

QT = core positioning dbus

HEADERS += \
    qgeopositioninfosource_geocluemaster.h \
    qgeosatelliteinfosource_geocluemaster.h \
    qgeopositioninfosourcefactory_geoclue.h \
    qgeocluemaster.h \
    geocluetypes.h

SOURCES += \
    qgeopositioninfosource_geocluemaster.cpp \
    qgeosatelliteinfosource_geocluemaster.cpp \
    qgeopositioninfosourcefactory_geoclue.cpp \
    qgeocluemaster.cpp \
    geocluetypes.cpp

QDBUSXML2CPP_INTERFACE_HEADER_FLAGS += "-N -i geocluetypes.h"
DBUS_INTERFACES += \
    org.freedesktop.Geoclue.MasterClient.xml \
    org.freedesktop.Geoclue.Master.xml \
    org.freedesktop.Geoclue.Position.xml \
    org.freedesktop.Geoclue.Velocity.xml \
    org.freedesktop.Geoclue.Satellite.xml \
    org.freedesktop.Geoclue.xml

OTHER_FILES += \
    $$DBUS_INTERFACES

INCLUDEPATH += $$QT.location.includes $$OUT_PWD

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryGeoclue
load(qt_plugin)
