CONFIG += testcase
TARGET = tst_qquickscreen
SOURCES += tst_qquickscreen.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

QT += core-private gui-private qml-private testlib quick-private
