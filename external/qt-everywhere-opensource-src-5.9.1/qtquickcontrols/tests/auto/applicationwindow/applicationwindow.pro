CONFIG += testcase
TARGET = tst_applicationwindow
SOURCES += tst_applicationwindow.cpp

macx:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private testlib

include (../shared/util.pri)

TESTDATA = data/*

OTHER_FILES += \
    data/basicapplicationwindow.qml

