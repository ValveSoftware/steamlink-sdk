CONFIG += testcase
TARGET = tst_qquicktext
macx:CONFIG -= app_bundle

SOURCES += tst_qquicktext.cpp

INCLUDEPATH += ../../shared/
HEADERS += ../../shared/testhttpserver.h
SOURCES += ../../shared/testhttpserver.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private network testlib
