TARGET = tst_qxmlserializer
CONFIG += testcase
QT += testlib
SOURCES += tst_qxmlserializer.cpp

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
