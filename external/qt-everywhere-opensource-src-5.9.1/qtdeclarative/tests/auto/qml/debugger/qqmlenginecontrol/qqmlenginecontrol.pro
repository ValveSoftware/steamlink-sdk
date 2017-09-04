CONFIG += testcase
TARGET = tst_qqmlenginecontrol
osx:CONFIG -= app_bundle

SOURCES += tst_qqmlenginecontrol.cpp

INCLUDEPATH += ../shared
include(../../../shared/util.pri)
include(../shared/debugutil.pri)

TESTDATA = data/*

QT += core qml testlib gui-private core-private

OTHER_FILES += \
    data/test.qml \
    data/exit.qml
