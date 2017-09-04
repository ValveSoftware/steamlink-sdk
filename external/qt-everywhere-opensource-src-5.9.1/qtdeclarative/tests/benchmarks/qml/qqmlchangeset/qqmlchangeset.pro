CONFIG += benchmark
TEMPLATE = app
TARGET = tst_qqmlchangeset
QT += qml quick-private testlib
osx:CONFIG -= app_bundle

SOURCES += tst_qqmlchangeset.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

