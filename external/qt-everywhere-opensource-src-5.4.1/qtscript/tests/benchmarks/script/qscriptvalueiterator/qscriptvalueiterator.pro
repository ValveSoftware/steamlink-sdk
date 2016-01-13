CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qscriptvalueiterator

SOURCES += tst_qscriptvalueiterator.cpp

QT = core script testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
