CONFIG += testcase
QT += qml testlib
TEMPLATE = app
TARGET = tst_qqmldebugtrace
macx:CONFIG -= app_bundle

SOURCES += tst_qqmldebugtrace.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
