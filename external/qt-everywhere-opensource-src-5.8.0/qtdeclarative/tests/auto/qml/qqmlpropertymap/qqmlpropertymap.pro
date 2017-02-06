CONFIG += testcase
TARGET = tst_qqmlpropertymap
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlpropertymap.cpp

include (../../shared/util.pri)

QT += core-private gui-private qml-private quick-private testlib
