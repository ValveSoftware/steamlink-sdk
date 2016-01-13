TARGET = tst_qabstracturiresolver
CONFIG += testcase
SOURCES += tst_qabstracturiresolver.cpp
HEADERS += TestURIResolver.h
QT = core testlib

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
