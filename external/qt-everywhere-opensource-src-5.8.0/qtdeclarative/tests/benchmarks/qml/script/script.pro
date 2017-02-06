CONFIG += benchmark
TEMPLATE = app
TARGET = tst_script
macx:CONFIG -= app_bundle
CONFIG += release

SOURCES += tst_script.cpp

QT += core-private gui-private  qml-private quick-private testlib

DEFINES += SRCDIR=\\\"$$PWD\\\"
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
