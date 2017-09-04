CONFIG += benchmark
TEMPLATE = app
TARGET = tst_creation
QT += core-private gui-private qml-private quick-private testlib
macx:CONFIG -= app_bundle

SOURCES += tst_creation.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"
