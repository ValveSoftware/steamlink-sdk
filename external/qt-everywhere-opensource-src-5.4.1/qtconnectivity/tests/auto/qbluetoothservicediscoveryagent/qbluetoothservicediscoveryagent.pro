SOURCES += tst_qbluetoothservicediscoveryagent.cpp
TARGET = tst_qbluetoothservicediscoveryagent
CONFIG += testcase

QT = core concurrent bluetooth testlib
blackberry {
    LIBS += -lbtapi
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
