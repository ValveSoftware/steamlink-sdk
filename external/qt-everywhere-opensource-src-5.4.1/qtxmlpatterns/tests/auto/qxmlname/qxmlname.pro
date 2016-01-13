TARGET = tst_qxmlname
CONFIG += testcase
QT += testlib
SOURCES += tst_qxmlname.cpp

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
