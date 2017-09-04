TEMPLATE = app
TARGET = tst_sensors2qmlapi

CONFIG += testcase
QT = core testlib sensors-private qml

SOURCES += tst_sensors2qmlapi.cpp \
           ./../../../src/imports/sensors/qmlsensorgesture.cpp \
           qtemplategestureplugin.cpp \
           qtemplaterecognizer.cpp

HEADERS += \
           ./../../../src/imports/sensors/qmlsensorgesture.h \
           qtemplategestureplugin.h \
           qtemplaterecognizer.h

