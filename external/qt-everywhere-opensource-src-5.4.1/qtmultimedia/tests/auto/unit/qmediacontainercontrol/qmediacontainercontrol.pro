CONFIG += testcase
TARGET = tst_qmediacontainercontrol

QT += multimedia-private testlib

SOURCES += tst_qmediacontainercontrol.cpp

include (../qmultimedia_common/mockcontainer.pri)

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
