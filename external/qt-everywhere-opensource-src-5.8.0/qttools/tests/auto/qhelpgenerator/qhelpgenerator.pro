TARGET = tst_qhelpgenerator
CONFIG += testcase

SOURCES += tst_qhelpgenerator.cpp
QT      += help-private sql testlib

DEFINES += SRCDIR=\\\"$$PWD\\\"
DEFINES += QT_USE_USING_NAMESPACE
