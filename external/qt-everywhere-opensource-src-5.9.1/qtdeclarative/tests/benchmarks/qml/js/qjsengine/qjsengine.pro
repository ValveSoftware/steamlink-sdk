CONFIG += benchmark
TEMPLATE = app
TARGET = tst_bench_qjsengine

SOURCES += tst_qjsengine.cpp

QT += qml testlib
macos:CONFIG -= app_bundle
