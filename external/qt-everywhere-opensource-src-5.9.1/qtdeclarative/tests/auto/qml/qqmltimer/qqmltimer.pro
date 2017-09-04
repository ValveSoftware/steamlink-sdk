CONFIG += testcase
TARGET = tst_qqmltimer
macx:CONFIG -= app_bundle

SOURCES += tst_qqmltimer.cpp

QT += core-private gui-private qml-private quick-private gui testlib
