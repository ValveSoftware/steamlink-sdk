CONFIG += testcase
TEMPLATE = app
TARGET = tst_creation
QT += core-private gui-private qml-private quick-private widgets testlib
macx:CONFIG -= app_bundle

SOURCES += tst_creation.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
