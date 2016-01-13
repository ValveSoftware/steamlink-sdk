CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qscriptqobject

SOURCES += tst_qscriptqobject.cpp

QT += script testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
