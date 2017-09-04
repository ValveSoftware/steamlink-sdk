CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qscriptvalue

SOURCES += tst_qscriptvalue.cpp

QT += script testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
