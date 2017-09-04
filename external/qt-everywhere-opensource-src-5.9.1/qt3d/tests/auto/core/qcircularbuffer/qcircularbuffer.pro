TARGET = tst_qcircularbuffer
CONFIG += testcase
TEMPLATE = app
QT += testlib 3dcore 3dcore-private

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += tst_qcircularbuffer.cpp
