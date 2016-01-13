CONFIG += testcase
TARGET = tst_qrcqml
QT += qml testlib
macx:CONFIG -= app_bundle

SOURCES += tst_qrcqml.cpp
RESOURCES = qrcqml.qrc

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
