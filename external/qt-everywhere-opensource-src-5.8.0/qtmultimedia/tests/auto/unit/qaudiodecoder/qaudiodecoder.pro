QT += multimedia multimedia-private testlib gui

TARGET = tst_qaudiodecoder

CONFIG += testcase

TEMPLATE = app

INCLUDEPATH += \
    ../../../../src/multimedia/audio

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockdecoder.pri)

SOURCES += tst_qaudiodecoder.cpp
