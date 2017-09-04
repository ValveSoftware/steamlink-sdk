TEMPLATE = app
CONFIG+=testcase
TARGET=tst_positionplugin

SOURCES += tst_positionplugin.cpp

CONFIG -= app_bundle

QT += positioning testlib
