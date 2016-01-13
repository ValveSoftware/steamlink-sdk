TARGET = tst_qxmlnamepool
CONFIG += testcase
QT += testlib
SOURCES += tst_qxmlnamepool.cpp

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
