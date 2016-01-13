TARGET = qtposition_cl
QT = core positioning

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryCL
load(qt_plugin)

INCLUDEPATH += $$QT.location.includes

OBJECTIVE_SOURCES += \
    qgeopositioninfosource_cl.mm \
    qgeopositioninfosourcefactory_cl.mm

HEADERS += \
    qgeopositioninfosource_cl_p.h \
    qgeopositioninfosourcefactory_cl.h

OTHER_FILES += \
    plugin.json

osx: LIBS += -framework Foundation
else: ios: LIBS += -framework CoreFoundation
LIBS += -framework CoreLocation
