TARGET = qtposition_blackberry
QT = core positioning

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryBb
load(qt_plugin)

INCLUDEPATH += $$QT.location.includes

LIBS = -lwmm -llocation_manager -lpps

SOURCES += qgeopositioninfosource_bb.cpp \
           qgeosatelliteinfosource_bb.cpp \
           locationmanagerutil_bb.cpp  \
           qgeopositioninfosourcefactory_bb.cpp
SOURCES += bb/ppsobject.cpp \
           bb/ppsattribute.cpp
HEADERS += qgeopositioninfosource_bb_p.h \
           qgeopositioninfosource_bb.h \
           qgeosatelliteinfosource_bb_p.h \
           qgeosatelliteinfosource_bb.h \
           locationmanagerutil_bb.h \
           qgeopositioninfosourcefactory_bb.h
HEADERS += bb/ppsobject.h \
           bb/ppsattribute_p.h

OTHER_FILES += \
    plugin.json
