CONFIG += console
CONFIG += testcase
CONFIG -= app_bundle

TEMPLATE = app

TARGET = tst_defaultmaskgenerator

QT = core testlib websockets websockets-private

SOURCES += tst_defaultmaskgenerator.cpp

requires(qtConfig(private_tests))
