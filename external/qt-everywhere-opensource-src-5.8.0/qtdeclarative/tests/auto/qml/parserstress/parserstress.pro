CONFIG += testcase
TARGET = tst_parserstress
macx:CONFIG -= app_bundle

SOURCES += tst_parserstress.cpp

TESTDATA = tests/*


QT += core-private gui-private qml-private testlib
