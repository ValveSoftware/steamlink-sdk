CONFIG += testcase
TARGET = tst_qmlmin
QT += qml testlib gui-private
macx:CONFIG -= app_bundle

SOURCES += tst_qmlmin.cpp
DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

cross_compile: DEFINES += QTEST_CROSS_COMPILED
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
