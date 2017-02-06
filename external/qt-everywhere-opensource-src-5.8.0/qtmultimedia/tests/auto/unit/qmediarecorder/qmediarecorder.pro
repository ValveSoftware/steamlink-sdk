CONFIG += testcase
TARGET = tst_qmediarecorder
QT += multimedia-private testlib
SOURCES += tst_qmediarecorder.cpp

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockrecorder.pri)
