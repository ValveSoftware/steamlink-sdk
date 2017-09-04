TARGET = tst_patternistexamples
CONFIG += testcase
SOURCES += tst_patternistexamples.cpp
QT += testlib

DEFINES += SOURCETREE=\\\"$$absolute_path(../../..)/\\\"

include (../xmlpatterns.pri)
