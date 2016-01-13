CONFIG += testcase
TARGET = tst_qdeclarativetextedit

QT += testlib declarative declarative-private gui gui-private widgets widgets-private network core-private
macx:CONFIG -= app_bundle

include(../shared/qdeclarativedatatest.pri)

SOURCES += tst_qdeclarativetextedit.cpp ../shared/testhttpserver.cpp
HEADERS += ../shared/testhttpserver.h

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
