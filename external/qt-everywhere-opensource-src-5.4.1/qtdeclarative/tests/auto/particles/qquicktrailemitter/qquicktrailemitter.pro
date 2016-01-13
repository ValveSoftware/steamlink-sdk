CONFIG += testcase
TARGET = tst_qquicktrailemitter
SOURCES += tst_qquicktrailemitter.cpp
macx:CONFIG -= app_bundle

win32-msvc2012:CONFIG += insignificant_test # QTBUG-33421

include (../../shared/util.pri)
TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private quickparticles-private testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
