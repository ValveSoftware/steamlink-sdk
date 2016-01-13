CONFIG += testcase
TARGET = tst_qqmlinspector

QT += qml testlib gui-private
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlinspector.cpp

INCLUDEPATH += ../shared
include(../../../shared/util.pri)
include(../shared/qqmlinspectorclient.pri)
include(../shared/debugutil.pri)

TESTDATA = data/*
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
