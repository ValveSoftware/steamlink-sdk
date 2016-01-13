CONFIG += testcase
TARGET = tst_qqmltypeloader
QT += qml testlib qml-private quick
macx:CONFIG -= app_bundle

SOURCES += tst_qqmltypeloader.cpp

include (../../shared/util.pri)

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
