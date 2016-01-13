CONFIG += testcase
TARGET = tst_qpacketprotocol
macx:CONFIG -= app_bundle

SOURCES += tst_qpacketprotocol.cpp

INCLUDEPATH += ../shared
include(../shared/debugutil.pri)

CONFIG += parallel_test
QT += qml network testlib gui-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
