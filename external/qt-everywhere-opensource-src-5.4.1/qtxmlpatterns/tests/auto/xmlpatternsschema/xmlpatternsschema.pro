TARGET = tst_xmlpatternsschema
CONFIG += testcase
QT += testlib

SOURCES += tst_xmlpatternsschema.cpp \

include (../xmlpatterns.pri)

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
