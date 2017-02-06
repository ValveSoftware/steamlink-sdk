CONFIG += testcase
TARGET = tst_qquickborderimage
macx:CONFIG -= app_bundle

HEADERS += ../../shared/testhttpserver.h
SOURCES += tst_qquickborderimage.cpp \
           ../../shared/testhttpserver.cpp

include (../../shared/util.pri)
include (../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private network testlib
