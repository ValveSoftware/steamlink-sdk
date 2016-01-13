TARGET = tst_qscriptextqobject
CONFIG += testcase
QT = core gui widgets script testlib
SOURCES  += tst_qscriptextqobject.cpp
include(../shared/util.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
