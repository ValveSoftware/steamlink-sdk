TARGET = qtsensorgestures_plugin
QT = core sensors

# Input
HEADERS += qtsensorgestureplugin.h \
    qcoversensorgesturerecognizer.h \
    qdoubletapsensorgesturerecognizer.h \
    qhoversensorgesturerecognizer.h \
    qfreefallsensorgesturerecognizer.h \
    qpickupsensorgesturerecognizer.h \
    qshake2recognizer.h \
    qslamgesturerecognizer.h \
    qturnoversensorgesturerecognizer.h \
    qtwistsensorgesturerecognizer.h \
    qwhipsensorgesturerecognizer.h \
    qtsensorgesturesensorhandler.h

SOURCES += qtsensorgestureplugin.cpp \
    qcoversensorgesturerecognizer.cpp \
    qdoubletapsensorgesturerecognizer.cpp \
    qhoversensorgesturerecognizer.cpp \
    qfreefallsensorgesturerecognizer.cpp \
    qpickupsensorgesturerecognizer.cpp \
    qshake2recognizer.cpp \
    qslamgesturerecognizer.cpp \
    qturnoversensorgesturerecognizer.cpp \
    qtwistsensorgesturerecognizer.cpp \
    qwhipsensorgesturerecognizer.cpp \
    qtsensorgesturesensorhandler.cpp

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = sensorgestures
PLUGIN_CLASS_NAME = QtSensorGesturePlugin
PLUGIN_EXTENDS = -
load(qt_plugin)
