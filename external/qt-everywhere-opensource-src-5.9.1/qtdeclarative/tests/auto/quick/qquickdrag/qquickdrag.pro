TARGET = tst_qquickdrag
CONFIG += testcase
macx:CONFIG -= app_bundle

SOURCES += tst_qquickdrag.cpp

QT += core-private gui-private qml-private quick-private network testlib
