TARGET = tst_qxmlschemavalidator
CONFIG += testcase
SOURCES += tst_qxmlschemavalidator.cpp
QT += network testlib

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
