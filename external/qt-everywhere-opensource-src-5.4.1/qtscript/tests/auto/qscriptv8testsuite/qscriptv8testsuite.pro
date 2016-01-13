TARGET = tst_qscriptv8testsuite
CONFIG += testcase
QT = core-private script testlib
SOURCES  += tst_qscriptv8testsuite.cpp
RESOURCES += qscriptv8testsuite.qrc
include(abstracttestsuite.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
