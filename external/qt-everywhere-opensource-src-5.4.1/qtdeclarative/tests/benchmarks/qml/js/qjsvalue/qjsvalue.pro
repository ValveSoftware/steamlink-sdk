CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qjsvalue

SOURCES += tst_qjsvalue.cpp

QT += qml testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
