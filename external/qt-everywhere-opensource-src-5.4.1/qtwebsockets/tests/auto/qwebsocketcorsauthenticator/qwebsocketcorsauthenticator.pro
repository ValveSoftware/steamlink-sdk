CONFIG += console
CONFIG += testcase
CONFIG -= app_bundle

TEMPLATE = app

TARGET = tst_qwebsocketcorsauthenticator

QT = core testlib websockets

SOURCES += tst_qwebsocketcorsauthenticator.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
