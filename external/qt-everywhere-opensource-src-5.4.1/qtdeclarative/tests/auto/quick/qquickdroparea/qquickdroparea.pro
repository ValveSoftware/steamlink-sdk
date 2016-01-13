TARGET = tst_qquickdroparea
CONFIG += testcase
macx:CONFIG -= app_bundle

SOURCES += tst_qquickdroparea.cpp

CONFIG += parallel_test

QT += core-private gui-private qml-private quick-private network testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
