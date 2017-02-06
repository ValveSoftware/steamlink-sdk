TARGET = qtposition_gypsy

QT = core positioning-private

HEADERS += \
    qgeosatelliteinfosource_gypsy_p.h \
    qgeopositioninfosourcefactory_gypsy.h

SOURCES += \
    qgeosatelliteinfosource_gypsy.cpp \
    qgeopositioninfosourcefactory_gypsy.cpp

QMAKE_USE_PRIVATE += gypsy

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryGypsy
load(qt_plugin)
