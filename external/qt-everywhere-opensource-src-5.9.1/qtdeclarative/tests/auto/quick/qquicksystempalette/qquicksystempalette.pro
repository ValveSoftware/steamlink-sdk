CONFIG += testcase
TARGET = tst_qquicksystempalette
macx:CONFIG -= app_bundle

SOURCES += tst_qquicksystempalette.cpp

QT += core-private gui-private qml-private quick-private testlib
qtHaveModule(widgets): QT += widgets
