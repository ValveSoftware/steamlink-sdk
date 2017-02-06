CONFIG += testcase
TARGET = tst_qquickwindow
SOURCES += tst_qquickwindow.cpp

include (../../shared/util.pri)
include(../shared/util.pri)

macx:CONFIG -= app_bundle

QT += core-private qml-private quick-private  testlib

TESTDATA = data/*

OTHER_FILES += \
    data/active.qml \
    data/AnimationsWhileHidden.qml \
    data/Headless.qml \
    data/showHideAnimate.qml \
    data/windoworder.qml
