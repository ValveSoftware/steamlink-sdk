TARGET = tst_qdds

QT = core gui testlib
CONFIG -= app_bundle
CONFIG += testcase

SOURCES += tst_qdds.cpp
RESOURCES += $$PWD/../../shared/images/dds.qrc
