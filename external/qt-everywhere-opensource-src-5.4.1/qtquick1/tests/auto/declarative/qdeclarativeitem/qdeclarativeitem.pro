CONFIG += testcase
TARGET = tst_qdeclarativeitem

QT += testlib declarative declarative-private gui gui-private widgets-private core-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeitem.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
