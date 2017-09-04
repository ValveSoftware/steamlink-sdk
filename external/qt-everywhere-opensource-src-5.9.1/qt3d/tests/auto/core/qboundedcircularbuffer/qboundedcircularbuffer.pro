TARGET = tst_qboundedcircularbuffer
CONFIG += testcase
TEMPLATE = app
QT += testlib 3dcore 3dcore-private

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += tst_qboundedcircularbuffer.cpp
