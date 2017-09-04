TARGET = tst_qabstractxmlnodemodel
CONFIG += testcase
QT += testlib
SOURCES += tst_qabstractxmlnodemodel.cpp    \
           LoadingModel.cpp                 \
           ../qxmlquery/TestFundament.cpp
HEADERS += TestNodeModel.h LoadingModel.h

TESTDATA = tree.xml

include (../xmlpatterns.pri)
