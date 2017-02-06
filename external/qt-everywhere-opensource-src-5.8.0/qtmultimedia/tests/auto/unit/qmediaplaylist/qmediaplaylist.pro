CONFIG += testcase
TARGET = tst_qmediaplaylist

include (../qmultimedia_common/mockplaylist.pri)

QT += multimedia-private testlib

HEADERS += \
    ../../../../src/plugins/m3u/qm3uhandler.h

SOURCES += \
    tst_qmediaplaylist.cpp \
    ../../../../src/plugins/m3u/qm3uhandler.cpp

INCLUDEPATH += ../../../../src/plugins/m3u

TESTDATA += testdata/*
