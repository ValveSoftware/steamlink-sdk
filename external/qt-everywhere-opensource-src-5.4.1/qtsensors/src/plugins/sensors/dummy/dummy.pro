TARGET = qtsensors_dummy
QT = sensors core

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = dummySensorPlugin
load(qt_plugin)

HEADERS += dummycommon.h\
           dummyaccelerometer.h\
           dummylightsensor.h

SOURCES += dummycommon.cpp\
           dummyaccelerometer.cpp\
           dummylightsensor.cpp\
           main.cpp

OTHER_FILES = plugin.json

unix:!mac:!qnx:!android:LIBS+=-lrt
