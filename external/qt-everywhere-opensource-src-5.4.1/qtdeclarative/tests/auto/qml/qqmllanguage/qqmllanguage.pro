CONFIG += testcase
TARGET = tst_qqmllanguage
macx:CONFIG -= app_bundle

SOURCES += tst_qqmllanguage.cpp \
           testtypes.cpp
HEADERS += testtypes.h

INCLUDEPATH += ../../shared/
HEADERS += ../../shared/testhttpserver.h
SOURCES += ../../shared/testhttpserver.cpp

TESTDATA = data/*

QT += core-private gui-private  qml-private network testlib

include (../../shared/util.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

OTHER_FILES += \
    data/readonlyObjectProperty.qml
