CONFIG += testcase
TARGET = tst_qqmlvaluetypes
macx:CONFIG -= app_bundle

HEADERS += testtypes.h

SOURCES += tst_qqmlvaluetypes.cpp \
           testtypes.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private gui testlib

DISTFILES += \
    data/customvaluetype.qml
