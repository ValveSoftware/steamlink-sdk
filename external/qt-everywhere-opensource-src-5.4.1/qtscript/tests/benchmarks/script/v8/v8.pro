CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_v8
RESOURCES += v8.qrc
SOURCES += tst_v8.cpp

QT = core script testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
