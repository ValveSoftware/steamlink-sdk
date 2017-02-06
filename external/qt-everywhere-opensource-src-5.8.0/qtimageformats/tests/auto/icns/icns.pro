TARGET = tst_qicns

QT = core gui testlib
CONFIG -= app_bundle
CONFIG += testcase

SOURCES += tst_qicns.cpp
RESOURCES += $$PWD/../../shared/images/icns.qrc
