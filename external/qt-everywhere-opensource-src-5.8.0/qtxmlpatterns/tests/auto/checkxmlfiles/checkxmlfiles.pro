TARGET = tst_checkxmlfiles
CONFIG += testcase
SOURCES += tst_checkxmlfiles.cpp \
           ../qxmlquery/TestFundament.cpp
QT = core gui testlib

include (../xmlpatterns.pri)

DEFINES += SOURCETREE=\\\"$$absolute_path(../../..)/\\\"
