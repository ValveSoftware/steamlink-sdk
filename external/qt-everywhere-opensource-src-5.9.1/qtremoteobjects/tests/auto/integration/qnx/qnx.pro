DEFINES += BACKEND=\\\"qnx\\\"
DEFINES += HOST_URL=\\\"qnx:replica\\\"
DEFINES += REGISTRY_URL=\\\"qnx:registry\\\"
CONFIG += testcase
TARGET = tst_integration_qnx
QT += testlib remoteobjects
QT -= gui

include(../template.pri)
