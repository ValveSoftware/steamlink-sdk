CONFIG += testcase
TARGET = tst_qqmlincubator
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlincubator.cpp \
           testtypes.cpp

HEADERS += testtypes.h

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test

QT += core-private gui-private  qml-private network testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
