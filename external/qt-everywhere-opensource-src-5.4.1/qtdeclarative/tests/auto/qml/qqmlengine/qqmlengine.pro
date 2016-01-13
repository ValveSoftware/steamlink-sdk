CONFIG += testcase
TARGET = tst_qqmlengine
macx:CONFIG -= app_bundle

include (../../shared/util.pri)

SOURCES += tst_qqmlengine.cpp

QT += core-private gui-private qml-private  network testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
