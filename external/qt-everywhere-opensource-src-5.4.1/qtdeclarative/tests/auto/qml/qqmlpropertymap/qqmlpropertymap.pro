CONFIG += testcase
TARGET = tst_qqmlpropertymap
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlpropertymap.cpp

include (../../shared/util.pri)

CONFIG += parallel_test

QT += core-private gui-private qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
