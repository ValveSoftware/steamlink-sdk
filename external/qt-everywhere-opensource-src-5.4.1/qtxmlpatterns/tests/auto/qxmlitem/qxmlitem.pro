TARGET = tst_qxmlitem
CONFIG += testcase
QT += testlib
SOURCES += tst_qxmlitem.cpp

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
