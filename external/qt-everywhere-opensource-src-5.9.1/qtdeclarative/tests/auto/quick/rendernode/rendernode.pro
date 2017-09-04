CONFIG += testcase
TARGET = tst_rendernode
SOURCES += tst_rendernode.cpp

macx:CONFIG -= app_bundle

TESTDATA = data/*

include(../../shared/util.pri)

QT += core-private gui-private  qml-private quick-private testlib

OTHER_FILES += \
    data/RenderOrder.qml \
    data/MessUpState.qml \
    data/matrix.qml

