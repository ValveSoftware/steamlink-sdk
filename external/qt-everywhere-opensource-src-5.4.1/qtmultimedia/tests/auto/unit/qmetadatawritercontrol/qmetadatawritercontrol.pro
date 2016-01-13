CONFIG += testcase
TARGET = tst_qmetadatawritercontrol

QT += multimedia-private testlib

SOURCES += tst_qmetadatawritercontrol.cpp

include (../qmultimedia_common/mockcontainer.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
