QT = core-private webchannel-private testlib

CONFIG += testcase strict_flags warn_on

INCLUDEPATH += ../../../src/webchannel
VPATH += ../../../src/webchannel

TARGET = tst_webchannel

SOURCES += \
    tst_webchannel.cpp

HEADERS += \
    tst_webchannel.h
