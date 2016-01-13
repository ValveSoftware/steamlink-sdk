TARGET = tst_xmlpatternsvalidator
CONFIG += testcase
QT += testlib
SOURCES += tst_xmlpatternsvalidator.cpp \
           ../qxmlquery/TestFundament.cpp

include (../xmlpatterns.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
