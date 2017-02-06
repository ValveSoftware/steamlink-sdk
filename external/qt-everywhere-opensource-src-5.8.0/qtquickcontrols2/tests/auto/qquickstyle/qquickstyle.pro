CONFIG += testcase
TARGET = tst_qquickstyle
SOURCES += tst_qquickstyle.cpp

osx:CONFIG -= app_bundle

QT += quickcontrols2 testlib
QT_PRIVATE += core-private gui-private quickcontrols2-private
