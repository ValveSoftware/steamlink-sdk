CONFIG += testcase
TARGET = tst_qquicktextinput
macx:CONFIG -= app_bundle

SOURCES += tst_qquicktextinput.cpp \
           ../../shared/testhttpserver.cpp

HEADERS += ../../shared/testhttpserver.h

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
