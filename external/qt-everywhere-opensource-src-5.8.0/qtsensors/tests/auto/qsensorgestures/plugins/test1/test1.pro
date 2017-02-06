TARGET  = qtsensorgestures_testplugin1

QT += sensors   sensorgestures

# Input
HEADERS +=  qtestsensorgestureplugindup_p.h \
                  qtestrecognizerdup.h \
                  qtest2recognizerdup.h
SOURCES += qtestsensorgestureplugindup.cpp \
                  qtestrecognizerdup.cpp \
                  qtest2recognizerduo.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

PLUGIN_TYPE = sensorgestures
PLUGIN_CLASS_NAME = QTestSensorGestureDupPlugin
PLUGIN_EXTENDS = -
load(qt_plugin)
