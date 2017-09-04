TARGET = qtsensors_linuxsys
QT = core sensors

OTHER_FILES = plugin.json

!android:LIBS += -lrt
HEADERS += linuxsysaccelerometer.h
SOURCES += linuxsysaccelerometer.cpp \
main.cpp

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = LinuxSensorPlugin
load(qt_plugin)
