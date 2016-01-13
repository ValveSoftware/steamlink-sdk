CONFIG += testcase
TARGET = tst_qdeclarativetext

QT += testlib declarative declarative-private gui gui-private widgets widgets-private core-private script-private
QT += network
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativetext.cpp

INCLUDEPATH += ../shared/
HEADERS += ../shared/testhttpserver.h
SOURCES += ../shared/testhttpserver.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

CONFIG += insignificant_test # QTBUG-21015
