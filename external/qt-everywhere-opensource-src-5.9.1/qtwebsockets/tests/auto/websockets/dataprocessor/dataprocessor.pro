CONFIG += console
CONFIG += testcase
CONFIG -= app_bundle

TEMPLATE = app

TARGET = tst_dataprocessor

QT = core testlib websockets websockets-private

SOURCES += tst_dataprocessor.cpp

requires(qtConfig(private_tests))
