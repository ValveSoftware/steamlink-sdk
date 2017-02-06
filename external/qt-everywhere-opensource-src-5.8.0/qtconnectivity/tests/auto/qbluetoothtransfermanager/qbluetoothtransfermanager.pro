SOURCES += tst_qbluetoothtransfermanager.cpp
TARGET=tst_qbluetoothtransfermanager
CONFIG += testcase
testcase.timeout = 250 # this test is slow

QT = core concurrent bluetooth testlib

TESTDATA += *.txt
