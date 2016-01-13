TARGET = qtsensors_simulator

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = SimulatorSensorPlugin
load(qt_plugin)

QT=core gui network sensors simulator

HEADERS += \
    simulatorcommon.h\
    simulatoraccelerometer.h\
    simulatorambientlightsensor.h\
    simulatorlightsensor.h\
    simulatorcompass.h\
    simulatorproximitysensor.h\
    simulatorirproximitysensor.h\
    simulatormagnetometer.h\
    qsensordata_simulator_p.h

SOURCES += \
    simulatorcommon.cpp\
    simulatoraccelerometer.cpp\
    simulatorambientlightsensor.cpp\
    simulatorlightsensor.cpp\
    simulatorcompass.cpp\
    simulatorproximitysensor.cpp\
    simulatorirproximitysensor.cpp\
    simulatormagnetometer.cpp\
    qsensordata_simulator.cpp\
    main.cpp

OTHER_FILES = plugin.json
