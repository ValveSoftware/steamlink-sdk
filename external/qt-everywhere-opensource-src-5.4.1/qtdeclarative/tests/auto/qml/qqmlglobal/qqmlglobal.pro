CONFIG += testcase
TARGET = tst_qqmlglobal
SOURCES += tst_qqmlglobal.cpp
macx:CONFIG -= app_bundle

CONFIG += parallel_test
QT += qml-private testlib  core-private gui-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
