CONFIG += testcase
TARGET = tst_qquickshadereffect
SOURCES += tst_qquickshadereffect.cpp

include (../../shared/util.pri)
macx:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private testlib
