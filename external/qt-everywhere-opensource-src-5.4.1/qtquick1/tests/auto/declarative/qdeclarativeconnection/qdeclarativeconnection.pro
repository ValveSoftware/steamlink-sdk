CONFIG += testcase
TARGET = tst_qdeclarativeconnection

QT += testlib declarative declarative-private gui core-private script-private gui-private widgets-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeconnection.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
