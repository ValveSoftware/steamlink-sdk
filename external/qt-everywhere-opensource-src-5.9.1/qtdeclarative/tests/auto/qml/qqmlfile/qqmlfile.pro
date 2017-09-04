CONFIG   += testcase
TARGET = tst_qqmlfile
SOURCES += tst_qqmlfile.cpp
macos:CONFIG -= app_bundle
QT       += qml testlib
