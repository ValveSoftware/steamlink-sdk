CONFIG += testcase
TARGET = tst_qradiotuner
QT += multimedia-private testlib
SOURCES += tst_qradiotuner.cpp

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockradio.pri)
