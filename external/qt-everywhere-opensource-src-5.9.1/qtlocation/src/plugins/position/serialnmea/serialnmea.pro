TARGET = qtposition_serialnmea

QT = core positioning serialport

HEADERS += \
    qgeopositioninfosourcefactory_serialnmea.h

SOURCES += \
    qgeopositioninfosourcefactory_serialnmea.cpp

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactorySerialNmea
load(qt_plugin)
