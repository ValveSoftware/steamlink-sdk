CONFIG += testcase
TARGET = tst_qquickitemparticle
SOURCES += tst_qquickitemparticle.cpp
macx:CONFIG -= app_bundle

include (../../shared/util.pri)
TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private quickparticles-private testlib

