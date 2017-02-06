CONFIG += testcase
TARGET = tst_qquickfontloader_static
macx:CONFIG -= app_bundle

SOURCES += tst_qquickfontloader_static.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private network testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
