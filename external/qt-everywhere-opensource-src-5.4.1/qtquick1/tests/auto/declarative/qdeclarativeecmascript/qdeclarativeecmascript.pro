CONFIG += testcase
TARGET = tst_qdeclarativeecmascript

QT += testlib declarative declarative-private script script-private network widgets core-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeecmascript.cpp \
           testtypes.cpp \
           ../shared/testhttpserver.cpp
HEADERS += testtypes.h \
           ../shared/testhttpserver.h
INCLUDEPATH += ../shared

# QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage
# LIBS += -lgcov

include(../shared/qdeclarativedatatest.pri)

CONFIG += parallel_test
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
