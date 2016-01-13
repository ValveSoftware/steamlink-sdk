CONFIG += testcase
CONFIG += parallel_test
TARGET = tst_qjsengine
QT +=  qml qml-private widgets testlib gui-private
macx:CONFIG -= app_bundle
SOURCES += tst_qjsengine.cpp
RESOURCES += qjsengine.qrc

TESTDATA = script/*
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
