TARGET = tst_qhelpindexmodel
CONFIG += testcase

SOURCES += tst_qhelpindexmodel.cpp
QT      += help sql testlib

DEFINES += SRCDIR=\\\"$$PWD\\\"
DEFINES += QT_USE_USING_NAMESPACE
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
