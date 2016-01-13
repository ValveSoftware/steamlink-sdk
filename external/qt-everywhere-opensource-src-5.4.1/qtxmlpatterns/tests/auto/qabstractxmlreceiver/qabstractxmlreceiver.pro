TARGET = tst_qabstractxmlreceiver
CONFIG += testcase
SOURCES += tst_qabstractxmlreceiver.cpp
QT = core testlib

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
