CONFIG += testcase
TARGET = tst_qdeclarativelanguage

QT += testlib declarative declarative-private script-private core-private
QT += script network
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativelanguage.cpp \
           testtypes.cpp
HEADERS += testtypes.h

HEADERS += ../shared/testhttpserver.h
SOURCES += ../shared/testhttpserver.cpp

include(../shared/qdeclarativedatatest.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
