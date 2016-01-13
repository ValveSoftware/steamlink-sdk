CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qscriptclass

SOURCES += tst_qscriptclass.cpp

QT += script testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
