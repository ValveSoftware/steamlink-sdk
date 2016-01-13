CONFIG += testcase
TARGET = tst_qqmlenginecontrol
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlenginecontrol.cpp

INCLUDEPATH += ../shared
include(../../../shared/util.pri)
include(../shared/debugutil.pri)

TESTDATA = data/*

QT += core qml testlib gui-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

OTHER_FILES += \
    data/test.qml \
    data/exit.qml
