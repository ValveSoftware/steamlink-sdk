CONFIG += testcase
CONFIG += parallel_test
TARGET = tst_qquicktimeline
macx:CONFIG -= app_bundle

SOURCES += tst_qquicktimeline.cpp
QT += core-private gui-private qml quick qml-private quick-private testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
