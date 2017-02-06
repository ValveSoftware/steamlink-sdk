CONFIG += testcase
TARGET = tst_qquickapplication
macx:CONFIG -= app_bundle

SOURCES += tst_qquickapplication.cpp
QT += core-private gui-private qml quick qml-private quick-private testlib

