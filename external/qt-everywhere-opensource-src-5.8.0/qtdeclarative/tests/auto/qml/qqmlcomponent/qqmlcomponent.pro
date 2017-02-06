CONFIG += testcase
TARGET = tst_qqmlcomponent
macx:CONFIG -= app_bundle

INCLUDEPATH += ../../shared/
SOURCES += tst_qqmlcomponent.cpp \
            ../../shared/testhttpserver.cpp

HEADERS += ../../shared/testhttpserver.h

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private network testlib
