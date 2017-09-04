CONFIG += testcase
TARGET = tst_qquickcumulativedirection
SOURCES += tst_qquickcumulativedirection.cpp
macx:CONFIG -= app_bundle

include (../../shared/util.pri)
TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private quickparticles-private testlib

