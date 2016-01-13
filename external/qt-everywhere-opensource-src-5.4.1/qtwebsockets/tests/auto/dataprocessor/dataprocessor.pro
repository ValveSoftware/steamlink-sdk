CONFIG += console
CONFIG += testcase
CONFIG -= app_bundle

TEMPLATE = app

TARGET = tst_dataprocessor

QT = core testlib websockets websockets-private

SOURCES += tst_dataprocessor.cpp

requires(contains(QT_CONFIG, private_tests))
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
