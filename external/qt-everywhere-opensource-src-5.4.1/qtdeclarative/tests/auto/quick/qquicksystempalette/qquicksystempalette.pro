CONFIG += testcase
TARGET = tst_qquicksystempalette
macx:CONFIG -= app_bundle

SOURCES += tst_qquicksystempalette.cpp

CONFIG += parallel_test
QT += core-private gui-private qml-private quick-private testlib
qtHaveModule(widgets): QT += widgets
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
