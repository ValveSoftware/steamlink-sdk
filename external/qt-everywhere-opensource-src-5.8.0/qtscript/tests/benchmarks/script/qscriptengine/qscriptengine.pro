CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qscriptengine

SOURCES += tst_qscriptengine.cpp

QT += script testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
