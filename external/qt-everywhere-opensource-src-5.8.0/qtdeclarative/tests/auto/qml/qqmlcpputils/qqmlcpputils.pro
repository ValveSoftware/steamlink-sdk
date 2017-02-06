CONFIG += testcase
TARGET = tst_qqmlcpputils
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlcpputils.cpp

QT += core-private gui-private qml-private testlib
