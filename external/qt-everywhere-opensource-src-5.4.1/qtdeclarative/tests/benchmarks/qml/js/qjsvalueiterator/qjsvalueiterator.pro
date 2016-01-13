CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qjsvalueiterator

SOURCES += tst_qjsvalueiterator.cpp

QT = core qml testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
