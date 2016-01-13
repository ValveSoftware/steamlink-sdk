CONFIG += testcase
TARGET = tst_qdeclarativedebugclient

QT += testlib network declarative declarative-private core-private gui-private widgets-private
macx:CONFIG -= app_bundle

HEADERS += ../shared/debugutil_p.h
SOURCES += tst_qdeclarativedebugclient.cpp \
           ../shared/debugutil.cpp

CONFIG += declarative_debug
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
