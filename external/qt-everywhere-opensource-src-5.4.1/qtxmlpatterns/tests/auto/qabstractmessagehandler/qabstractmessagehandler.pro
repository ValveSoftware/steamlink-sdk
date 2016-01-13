TARGET = tst_qabstractmessagehandler
CONFIG += testcase
SOURCES += tst_qabstractmessagehandler.cpp
QT = core testlib

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
