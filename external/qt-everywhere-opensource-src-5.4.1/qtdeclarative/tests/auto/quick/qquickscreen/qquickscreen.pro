CONFIG += testcase
TARGET = tst_qquickscreen
SOURCES += tst_qquickscreen.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

CONFIG += parallel_test
QT += core-private gui-private qml-private testlib quick-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
