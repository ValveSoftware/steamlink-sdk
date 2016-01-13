CONFIG += testcase
TARGET = tst_qmetadatareadercontrol

QT += multimedia-private testlib

SOURCES += tst_qmetadatareadercontrol.cpp

include (../qmultimedia_common/mockcontainer.pri)

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
