TARGET = tst_qtga

QT = core gui testlib
CONFIG -= app_bundle
CONFIG += testcase

SOURCES += tst_qtga.cpp
RESOURCES += $$PWD/../../shared/images/tga.qrc
