CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_sunspider
RESOURCES += sunspider.qrc
SOURCES += tst_sunspider.cpp

QT = core script testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
