CONFIG += testcase
TARGET = tst_qquickanimatedsprite
SOURCES += tst_qquickanimatedsprite.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private network testlib

OTHER_FILES += \
    data/largeAnimation.qml
