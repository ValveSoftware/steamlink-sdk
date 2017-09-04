CONFIG += testcase
TARGET = tst_qqmllocale
macx:CONFIG -= app_bundle

SOURCES += tst_qqmllocale.cpp

include (../../shared/util.pri)

TESTDATA = data/*


QT += qml testlib gui-private
