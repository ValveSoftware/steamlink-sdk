TARGET = tst_qxmlquery
CONFIG += testcase
SOURCES += tst_qxmlquery.cpp MessageValidator.cpp TestFundament.cpp
HEADERS += PushBaseliner.h                              \
           MessageSilencer.h                            \
           ../qsimplexmlnodemodel/TestSimpleNodeModel.h \
           MessageValidator.h                           \
           NetworkOverrider.h

RESOURCES = input.qrc

QT += network testlib

TESTDATA = data/* pushBaselines/* input.xml

include (../xmlpatterns.pri)
