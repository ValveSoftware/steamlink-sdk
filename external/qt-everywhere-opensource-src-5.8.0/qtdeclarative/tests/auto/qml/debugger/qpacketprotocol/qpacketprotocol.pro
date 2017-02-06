CONFIG += testcase
TARGET = tst_qpacketprotocol
osx:CONFIG -= app_bundle

SOURCES += tst_qpacketprotocol.cpp

INCLUDEPATH += ../shared
include(../shared/debugutil.pri)

QT += qml network testlib gui-private core-private
