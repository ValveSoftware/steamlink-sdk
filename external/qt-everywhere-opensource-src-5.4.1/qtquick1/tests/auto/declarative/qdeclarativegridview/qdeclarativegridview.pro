CONFIG += testcase
TARGET = tst_qdeclarativegridview

QT += testlib declarative declarative-private widgets widgets-private gui gui-private core-private script-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativegridview.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
