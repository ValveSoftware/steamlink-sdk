CONFIG += testcase
TARGET = tst_qdeclarativedebughelper

QT += testlib network declarative declarative-private script script-private core-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativedebughelper.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
