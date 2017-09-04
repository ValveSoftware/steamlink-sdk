TARGET = tst_qxmlformatter
CONFIG += testcase
QT += testlib
SOURCES += tst_qxmlformatter.cpp

TESTDATA = baselines/* input/*

include (../xmlpatterns.pri)
