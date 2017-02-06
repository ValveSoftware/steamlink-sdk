CONFIG += testcase
TARGET = tst_nokeywords
macx:CONFIG   -= app_bundle

SOURCES += tst_nokeywords.cpp

CONFIG+=parallel_test

QT += quick core-private gui-private qml-private quick-private testlib
