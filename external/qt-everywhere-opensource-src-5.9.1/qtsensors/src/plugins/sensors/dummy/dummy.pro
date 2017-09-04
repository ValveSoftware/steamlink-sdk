TARGET = qtsensors_dummy
QT = sensors core

HEADERS += dummycommon.h\
           dummyaccelerometer.h\
           dummylightsensor.h

SOURCES += dummycommon.cpp\
           dummyaccelerometer.cpp\
           dummylightsensor.cpp\
           main.cpp

OTHER_FILES = plugin.json

unix:!darwin:!qnx:!android:!openbsd: LIBS += -lrt

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = dummySensorPlugin
load(qt_plugin)
