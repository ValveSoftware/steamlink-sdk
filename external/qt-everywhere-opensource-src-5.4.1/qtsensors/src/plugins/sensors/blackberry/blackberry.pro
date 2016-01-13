TARGET = qtsensors_blackberry
QT = sensors core
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = BbSensorPlugin
load(qt_plugin)

config_bbsensor_header {
    DEFINES += HAVE_NDK_SENSOR_H
}
config_bbsensor_compass {
    DEFINES += HAVE_COMPASS_SENSOR
}

config_bbsensor_holster {
    DEFINES += HAVE_HOLSTER_SENSOR
}

HEADERS += bbsensorbackend.h \
    bbaccelerometer.h \
    bbaltimeter.h \
    bbambientlightsensor.h \
    bbcompass.h \
    bbdistancesensor.h \
    bbgyroscope.h \
    bbirproximitysensor.h \
    bblightsensor.h \
    bbmagnetometer.h \
    bborientationsensor.h \
    bbpressuresensor.h \
    bbproximitysensor.h \
    bbrotationsensor.h \
    bbtemperaturesensor.h \
    bbguihelper.h \
    bbutil.h

SOURCES += bbsensorbackend.cpp \
    bbaccelerometer.cpp \
    bbaltimeter.cpp \
    bbambientlightsensor.cpp \
    bbcompass.cpp \
    bbdistancesensor.cpp \
    bbgyroscope.cpp \
    bbirproximitysensor.cpp \
    bblightsensor.cpp \
    bbmagnetometer.cpp \
    bborientationsensor.cpp \
    bbpressuresensor.cpp \
    bbproximitysensor.cpp \
    bbrotationsensor.cpp \
    bbtemperaturesensor.cpp \
    bbguihelper.cpp \
    bbutil.cpp \
    main.cpp

config_bbsensor_holster {
    HEADERS += bbholstersensor.h
    SOURCES += bbholstersensor.cpp
}

OTHER_FILES = plugin.json
