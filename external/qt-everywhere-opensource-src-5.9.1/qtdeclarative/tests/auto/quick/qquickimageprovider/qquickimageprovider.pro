CONFIG += testcase
TARGET = tst_qquickimageprovider
macx:CONFIG -= app_bundle

SOURCES += tst_qquickimageprovider.cpp

QT += core-private gui-private qml-private quick-private network testlib
