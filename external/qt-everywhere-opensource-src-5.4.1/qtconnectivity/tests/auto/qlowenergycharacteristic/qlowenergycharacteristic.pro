SOURCES += tst_qlowenergycharacteristic.cpp
TARGET = tst_qlowenergycharacteristic
CONFIG += testcase

QT = core bluetooth testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
blackberry {
    LIBS += -lbtapi
}
