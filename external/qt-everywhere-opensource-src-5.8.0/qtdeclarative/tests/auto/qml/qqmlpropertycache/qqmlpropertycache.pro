CONFIG += testcase
TARGET = tst_qqmlpropertycache
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlpropertycache.cpp

QT += core-private gui-private qml-private testlib
