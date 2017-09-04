CONFIG += testcase
TARGET = tst_qquickanimatedimage
HEADERS += ../../shared/testhttpserver.h
SOURCES += tst_qquickanimatedimage.cpp \
           ../../shared/testhttpserver.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private network testlib
