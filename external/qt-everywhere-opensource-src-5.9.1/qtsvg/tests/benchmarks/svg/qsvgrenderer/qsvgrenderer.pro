CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qsvgrenderer

SOURCES += tst_qsvgrenderer.cpp
RESOURCES += qsvgrenderer.qrc

QT += svg testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
