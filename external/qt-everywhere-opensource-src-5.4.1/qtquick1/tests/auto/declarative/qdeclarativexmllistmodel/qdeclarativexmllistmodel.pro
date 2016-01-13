CONFIG += testcase
TARGET = tst_qdeclarativexmllistmodel

QT += testlib declarative declarative-private script gui network core-private script-private xmlpatterns
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativexmllistmodel.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
