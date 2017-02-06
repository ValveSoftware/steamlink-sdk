CONFIG += testcase
TARGET = tst_qquickangleddirection
SOURCES += tst_qquickangleddirection.cpp
macx:CONFIG -= app_bundle

include (../../shared/util.pri)
TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private quickparticles-private testlib

