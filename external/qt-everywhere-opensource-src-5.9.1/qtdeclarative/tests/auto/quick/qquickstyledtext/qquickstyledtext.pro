CONFIG += testcase
TARGET = tst_qquickstyledtext
macx:CONFIG -= app_bundle

SOURCES += tst_qquickstyledtext.cpp

QT += core-private gui-private qml-private quick-private network testlib
