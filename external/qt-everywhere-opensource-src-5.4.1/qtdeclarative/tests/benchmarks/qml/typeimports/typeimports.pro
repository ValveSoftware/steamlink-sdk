CONFIG += testcase
TEMPLATE = app
TARGET = tst_typeimports
QT += qml testlib
macx:CONFIG -= app_bundle

SOURCES += tst_typeimports.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
