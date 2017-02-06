CONFIG += testcase
TARGET = tst_geometry
macx:CONFIG   -= app_bundle

SOURCES += tst_geometry.cpp

CONFIG+=parallel_test

QT += core-private gui-private qml-private quick-private testlib
