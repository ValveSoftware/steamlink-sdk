CONFIG += testcase
testcase.timeout = 900 # this test is slow
TARGET = tst_qquicklistview
macx:CONFIG -= app_bundle

HEADERS += incrementalmodel.h \
           proxytestinnermodel.h \
           randomsortmodel.h
SOURCES += tst_qquicklistview.cpp \
           incrementalmodel.cpp \
           proxytestinnermodel.cpp \
           randomsortmodel.cpp


include (../../shared/util.pri)
include (../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private  testlib

