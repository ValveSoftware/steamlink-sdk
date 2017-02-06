CONFIG += benchmark
TEMPLATE = app
TARGET = tst_bench_qjsvalue
INCLUDEPATH += .
macx:CONFIG -= app_bundle
CONFIG += release

SOURCES += tst_qjsvalue.cpp

QT += core-private  qml-private testlib
