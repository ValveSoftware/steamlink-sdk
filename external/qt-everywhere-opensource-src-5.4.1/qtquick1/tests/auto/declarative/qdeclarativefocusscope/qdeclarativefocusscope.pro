CONFIG += testcase
TARGET = tst_qdeclarativefocusscope

QT += testlib declarative declarative-private widgets
SOURCES += tst_qdeclarativefocusscope.cpp
macx:CONFIG -= app_bundle

DEFINES += SRCDIR=\\\"$$PWD\\\"

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
