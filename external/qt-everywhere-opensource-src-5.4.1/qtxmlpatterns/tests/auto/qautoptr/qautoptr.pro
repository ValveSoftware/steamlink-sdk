TARGET = tst_qautoptr
CONFIG += testcase
SOURCES += tst_qautoptr.cpp
QT = core testlib
include(../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
