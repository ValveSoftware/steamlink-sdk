CONFIG += testcase
TARGET = tst_qqmlvaluetypeproviders
macx:CONFIG -= app_bundle

HEADERS += testtypes.h

SOURCES += tst_qqmlvaluetypeproviders.cpp \
           testtypes.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private gui testlib
