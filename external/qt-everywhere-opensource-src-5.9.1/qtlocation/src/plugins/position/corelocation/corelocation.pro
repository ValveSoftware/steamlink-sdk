TARGET = qtposition_cl

QT = core positioning

OBJECTIVE_SOURCES += \
    qgeopositioninfosource_cl.mm \
    qgeopositioninfosourcefactory_cl.mm

HEADERS += \
    qgeopositioninfosource_cl_p.h \
    qgeopositioninfosourcefactory_cl.h

OTHER_FILES += \
    plugin.json

LIBS += -framework Foundation -framework CoreLocation

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryCL
load(qt_plugin)
