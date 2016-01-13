CONFIG += testcase
TARGET = tst_qqmlsqldatabase
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlsqldatabase.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private sql testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
