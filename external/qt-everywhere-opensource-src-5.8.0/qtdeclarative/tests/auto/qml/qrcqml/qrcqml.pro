CONFIG += testcase
TARGET = tst_qrcqml
QT += qml testlib
macx:CONFIG -= app_bundle

SOURCES += tst_qrcqml.cpp
RESOURCES = qrcqml.qrc
