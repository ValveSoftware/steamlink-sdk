CONFIG += testcase
TARGET = tst_qqmldirparser
QT += qml testlib qml-private
macx:CONFIG -= app_bundle

SOURCES += tst_qqmldirparser.cpp

include (../../shared/util.pri)

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
