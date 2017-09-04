CONFIG += testcase
TARGET = tst_qqmldirparser
QT += qml testlib qml-private
macx:CONFIG -= app_bundle

SOURCES += tst_qqmldirparser.cpp

include (../../shared/util.pri)
