TEMPLATE = app
TARGET = tst_gifs

QT += quick testlib
CONFIG += testcase
osx:CONFIG -= app_bundle

HEADERS += \
    $$PWD/gifrecorder.h \
    $$PWD/eventcapturer.h \
    capturedevent.h

SOURCES += \
    $$PWD/tst_gifs.cpp \
    $$PWD/gifrecorder.cpp \
    $$PWD/eventcapturer.cpp \
    capturedevent.cpp

TESTDATA += \
    $$PWD/data/*
