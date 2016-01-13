CONFIG += testcase
TARGET = tst_qdeclarativelistview

QT += testlib declarative declarative-private widgets widgets-private gui-private core-private script-private
macx:CONFIG -= app_bundle

HEADERS += incrementalmodel.h
SOURCES += tst_qdeclarativelistview.cpp incrementalmodel.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
