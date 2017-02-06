CONFIG += testcase
TARGET = tst_qqmlglobal
SOURCES += tst_qqmlglobal.cpp
macx:CONFIG -= app_bundle

QT += qml-private testlib  core-private gui-private
