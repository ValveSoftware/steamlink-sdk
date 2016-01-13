TARGET = tst_patternistexamplefiletree
CONFIG += testcase
SOURCES += tst_patternistexamplefiletree.cpp
QT = core testlib

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
