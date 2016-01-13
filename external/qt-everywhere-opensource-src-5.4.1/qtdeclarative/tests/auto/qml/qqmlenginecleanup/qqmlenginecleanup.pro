CONFIG += testcase
TARGET = tst_qqmlenginecleanup
macx:CONFIG -= app_bundle

include (../../shared/util.pri)

SOURCES += tst_qqmlenginecleanup.cpp

QT += testlib qml
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
