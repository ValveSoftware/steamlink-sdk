CONFIG += testcase
TARGET = tst_qquicktextedit
macx:CONFIG -= app_bundle

SOURCES += tst_qquicktextedit.cpp \
           ../../shared/testhttpserver.cpp

HEADERS += ../../shared/testhttpserver.h

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private network-private testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

mac:CONFIG += insignificant_test # QTBUG-27740
win32:CONFIG += insignificant_test # QTBUG-32540
