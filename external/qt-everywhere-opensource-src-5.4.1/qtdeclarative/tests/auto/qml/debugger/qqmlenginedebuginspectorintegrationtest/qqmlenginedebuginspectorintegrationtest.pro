CONFIG += testcase
TARGET = tst_qqmlenginedebuginspectorintegration

QT += qml testlib gui-private
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlenginedebuginspectorintegration.cpp

INCLUDEPATH += ../shared
include(../../../shared/util.pri)
include(../shared/qqmlinspectorclient.pri)
include(../shared/qqmlenginedebugclient.pri)
include(../shared/debugutil.pri)

TESTDATA = data/*
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
