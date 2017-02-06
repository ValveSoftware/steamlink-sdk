TARGET = tst_xmlpatterns
CONFIG += testcase
QT += network testlib
SOURCES += tst_xmlpatterns.cpp \
           ../qxmlquery/TestFundament.cpp

include (../xmlpatterns.pri)
