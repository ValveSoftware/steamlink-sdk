SOURCES += tst_qbluetoothsocket.cpp
TARGET = tst_qbluetoothsocket
CONFIG += testcase
testcase.timeout = 250 # this test is slow

QT = core concurrent network bluetooth-private testlib
osx:QT += widgets

OTHER_FILES += \
    README.txt

osx {
    DEFINES += QT_OSX_BLUETOOTH
} else: android {
    DEFINES += QT_ANDROID_BLUETOOTH
} else: qtConfig(bluez):qtHaveModule(dbus) {
    DEFINES += QT_BLUEZ_BLUETOOTH
}
