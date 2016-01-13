#-------------------------------------------------
#
# Project created by QtCreator 2012-02-07T15:27:07
#
#-------------------------------------------------

QT += multimedia multimedia-private testlib gui

TARGET = tst_qaudiodecoder

CONFIG += testcase no_private_qt_headers_warning

TEMPLATE = app

INCLUDEPATH += \
    ../../../../src/multimedia/audio

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockdecoder.pri)

SOURCES += tst_qaudiodecoder.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
