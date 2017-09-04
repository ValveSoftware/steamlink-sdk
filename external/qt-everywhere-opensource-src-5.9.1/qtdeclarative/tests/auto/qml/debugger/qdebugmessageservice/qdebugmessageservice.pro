CONFIG += testcase
TARGET = tst_qdebugmessageservice
QT += qml network testlib gui-private core-private
osx:CONFIG -= app_bundle

SOURCES +=     tst_qdebugmessageservice.cpp

INCLUDEPATH += ../shared
include(../../../shared/util.pri)
include(../shared/debugutil.pri)

TESTDATA = data/*

OTHER_FILES += data/test.qml
