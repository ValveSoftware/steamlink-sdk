QT       += testlib

TARGET = tst_imagecompare
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

SOURCES += tst_imagecompare.cpp \
    imagecompare.cpp \
    main.cpp

DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += \
    imagecompare.h \
    tst_imagecompare.h
