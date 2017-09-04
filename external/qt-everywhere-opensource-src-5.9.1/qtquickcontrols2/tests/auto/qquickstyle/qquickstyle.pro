CONFIG += testcase
TARGET = tst_qquickstyle
SOURCES += tst_qquickstyle.cpp

macos:CONFIG -= app_bundle

QT += quickcontrols2 testlib
QT_PRIVATE += core-private gui-private quickcontrols2-private

TESTDATA = $$PWD/data/*
