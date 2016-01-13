CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qjsengine

SOURCES += tst_qjsengine.cpp

QT += qml testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
