TARGET = qtsensorgestures_shakeplugin
QT = core sensors

# Input
HEADERS += qshakesensorgestureplugin.h \
           qshakerecognizer.h
SOURCES += qshakesensorgestureplugin.cpp \
           qshakerecognizer.cpp

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = sensorgestures
PLUGIN_CLASS_NAME = QShakeSensorGesturePlugin
PLUGIN_EXTENDS = -
load(qt_plugin)
