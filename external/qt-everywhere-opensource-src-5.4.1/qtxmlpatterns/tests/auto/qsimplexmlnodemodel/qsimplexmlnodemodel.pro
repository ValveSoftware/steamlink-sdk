TARGET = tst_qsimplexmlnodemodel
CONFIG += testcase
QT += testlib
SOURCES += tst_qsimplexmlnodemodel.cpp
HEADERS += TestSimpleNodeModel.h
include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
