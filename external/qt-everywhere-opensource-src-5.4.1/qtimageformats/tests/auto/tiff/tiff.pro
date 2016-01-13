TARGET = tst_qtiff

QT = core gui testlib
CONFIG -= app_bundle
CONFIG += testcase

SOURCES += tst_qtiff.cpp
RESOURCES += $$PWD/../../shared/images/tiff.qrc
