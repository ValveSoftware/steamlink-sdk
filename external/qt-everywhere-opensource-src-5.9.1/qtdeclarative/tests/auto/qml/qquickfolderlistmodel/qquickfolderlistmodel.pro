CONFIG += testcase
TARGET = tst_qquickfolderlistmodel
macx:CONFIG -= app_bundle

SOURCES += tst_qquickfolderlistmodel.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private testlib

RESOURCES += data/introspect.qrc
