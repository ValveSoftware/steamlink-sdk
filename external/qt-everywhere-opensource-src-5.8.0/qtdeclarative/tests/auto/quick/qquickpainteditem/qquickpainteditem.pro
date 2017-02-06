TARGET = tst_qquickpainteditem
CONFIG += testcase
macx:CONFIG -= app_bundle

SOURCES += tst_qquickpainteditem.cpp

QT += core-private gui-private qml-private quick-private  network testlib
