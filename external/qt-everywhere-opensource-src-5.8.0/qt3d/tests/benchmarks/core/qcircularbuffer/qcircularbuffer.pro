TARGET = tst_bench_qcircularbuffer
CONFIG += release
TEMPLATE = app
QT += testlib 3dcore 3dcore-private

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += tst_bench_qcircularbuffer.cpp
