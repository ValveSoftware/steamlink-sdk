CONFIG += console
CONFIG += c++11
CONFIG += testcase
CONFIG -= app_bundle

TEMPLATE = app

TARGET = tst_websockets

QT = core network websockets testlib

SOURCES += tst_websockets.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
