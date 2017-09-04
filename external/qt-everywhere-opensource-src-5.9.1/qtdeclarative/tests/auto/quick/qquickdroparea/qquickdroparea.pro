TARGET = tst_qquickdroparea
CONFIG += testcase
macx:CONFIG -= app_bundle

SOURCES += tst_qquickdroparea.cpp

QT += core-private gui-private qml-private quick-private network testlib
