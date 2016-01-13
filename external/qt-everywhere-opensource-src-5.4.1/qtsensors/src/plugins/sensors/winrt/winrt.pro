TARGET = qtsensors_winrt
QT = sensors core

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = WinRtSensorPlugin
load(qt_plugin)

HEADERS += \
    winrtaccelerometer.h \
    winrtambientlightsensor.h \
    winrtcommon.h \
    winrtcompass.h \
    winrtorientationsensor.h \
    winrtrotationsensor.h \
    winrtgyroscope.h

SOURCES += \
    main.cpp \
    winrtaccelerometer.cpp \
    winrtambientlightsensor.cpp \
    winrtcommon.cpp \
    winrtcompass.cpp \
    winrtorientationsensor.cpp \
    winrtrotationsensor.cpp \
    winrtgyroscope.cpp

OTHER_FILES = plugin.json
