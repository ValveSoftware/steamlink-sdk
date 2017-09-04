DEFINES += BACKEND=\\\"local\\\"
DEFINES += HOST_URL=\\\"local:replica_local_integration\\\"
DEFINES += REGISTRY_URL=\\\"local:registry_local_integration\\\"
CONFIG += testcase
TARGET = tst_integration_local
QT += testlib remoteobjects
QT -= gui

include(../template.pri)
