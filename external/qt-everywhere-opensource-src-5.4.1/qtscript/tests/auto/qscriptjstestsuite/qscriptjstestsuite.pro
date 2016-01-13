TARGET = tst_qscriptjstestsuite
CONFIG += testcase
QT = core-private script testlib
SOURCES  += tst_qscriptjstestsuite.cpp
RESOURCES += qscriptjstestsuite.qrc
include(../qscriptv8testsuite/abstracttestsuite.pri)

DEFINES += SRCDIR=\\\"$$PWD\\\"

wince* {
testFiles.files = tests
testFiles.path = .
DEPLOYMENT += testFiles
}
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
win32:CONFIG += insignificant_test # QTBUG-36632
