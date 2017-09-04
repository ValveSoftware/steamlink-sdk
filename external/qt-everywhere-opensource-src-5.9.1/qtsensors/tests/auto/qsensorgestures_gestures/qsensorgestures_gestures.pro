TEMPLATE = app
TARGET = tst_sensorgestures_gestures
CONFIG += testcase

QT += core testlib sensors-private
QT -= gui

CONFIG += console
CONFIG -= app_bundle


SOURCES += tst_sensorgestures_gestures.cpp \
    mockcommon.cpp

DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += \
    mockcommon.h \
    mockbackends.h

TESTDATA += mock_data dataset2_mock_data
