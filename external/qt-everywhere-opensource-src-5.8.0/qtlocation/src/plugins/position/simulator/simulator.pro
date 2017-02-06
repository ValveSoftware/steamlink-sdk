TARGET = qtposition_simulator

QT = core network positioning simulator

INCLUDEPATH += ../../../positioning

DEFINES += QT_SIMULATOR
SOURCES += qgeopositioninfosource_simulator.cpp \
            qgeosatelliteinfosource_simulator.cpp \
            qlocationconnection_simulator.cpp \
    qgeopositioninfosourcefactory_simulator.cpp
HEADERS += qgeopositioninfosource_simulator_p.h \
            qgeosatelliteinfosource_simulator_p.h \
            qlocationconnection_simulator_p.h \
    qgeopositioninfosourcefactory_simulator.h

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactorySimulator
load(qt_plugin)
