CONFIG += testcase
TARGET = tst_qmlmin
QT += qml testlib gui-private
macx:CONFIG -= app_bundle

SOURCES += tst_qmlmin.cpp
DEFINES += SRCDIR=\\\"$$PWD\\\"


cross_compile: DEFINES += QTEST_CROSS_COMPILED
