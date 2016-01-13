CONFIG += testcase
TARGET = tst_qdeclarativelistmodel

QT += testlib declarative declarative-private script-private core-private gui-private widgets-private
QT += script
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativelistmodel.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
