TARGET = tst_qabstractxmlforwarditerator
CONFIG += testcase
SOURCES += tst_qabstractxmlforwarditerator.cpp
QT = core testlib

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
