CONFIG += console
CONFIG += testcase
CONFIG -= app_bundle

QT       = core testlib websockets

TARGET = tst_qwebsocketserver

TEMPLATE = app

SOURCES += tst_qwebsocketserver.cpp

