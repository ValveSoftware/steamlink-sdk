SOURCES += tst_qbluetoothdeviceinfo.cpp
TARGET=tst_qbluetoothdeviceinfo
CONFIG += testcase

QT = core concurrent bluetooth testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
blackberry {
    LIBS += -lbtapi
}
