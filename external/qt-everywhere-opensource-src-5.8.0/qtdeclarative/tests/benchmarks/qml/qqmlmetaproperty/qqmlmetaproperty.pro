CONFIG += benchmark
TEMPLATE = app
TARGET = tst_qqmlmetaproperty
QT += qml testlib
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlmetaproperty.cpp

# Define SRCDIR equal to test's source directory
DEFINES += SRCDIR=\\\"$$PWD\\\"
