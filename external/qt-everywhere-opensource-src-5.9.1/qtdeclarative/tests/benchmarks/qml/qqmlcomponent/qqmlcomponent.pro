CONFIG += benchmark
TEMPLATE = app
TARGET = tst_qqmlcomponent
QT += qml testlib
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlcomponent.cpp testtypes.cpp
HEADERS += testtypes.h

# Define SRCDIR equal to test's source directory
DEFINES += SRCDIR=\\\"$$PWD\\\"
