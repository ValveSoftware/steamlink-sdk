CONFIG += testcase
TARGET = tst_customcontrolsstyle
SOURCES += tst_customcontrolsstyle.cpp

include (../shared/util.pri)

osx:CONFIG -= app_bundle

QT += core-private qml-private quick-private testlib

TESTDATA = data/*

OTHER_FILES += \
    data/TestComponent.qml

RESOURCES += \
    style.qrc
