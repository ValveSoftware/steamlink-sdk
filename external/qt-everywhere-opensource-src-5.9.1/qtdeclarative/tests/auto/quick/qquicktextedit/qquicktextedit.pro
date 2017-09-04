CONFIG += testcase
TARGET = tst_qquicktextedit
macx:CONFIG -= app_bundle

SOURCES += tst_qquicktextedit.cpp \
           ../../shared/testhttpserver.cpp

HEADERS += ../../shared/testhttpserver.h

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private network-private testlib

osx: LIBS += -framework AppKit
