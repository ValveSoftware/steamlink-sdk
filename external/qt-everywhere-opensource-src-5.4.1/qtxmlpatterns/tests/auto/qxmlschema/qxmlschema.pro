TARGET = tst_qxmlschema
CONFIG += testcase
SOURCES += tst_qxmlschema.cpp
QT += network testlib

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
