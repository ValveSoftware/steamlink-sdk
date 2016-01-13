CONFIG += testcase
TARGET = tst_qquickshadereffect
SOURCES += tst_qquickshadereffect.cpp

include (../../shared/util.pri)
macx:CONFIG -= app_bundle

CONFIG += parallel_test
QT += core-private gui-private qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
