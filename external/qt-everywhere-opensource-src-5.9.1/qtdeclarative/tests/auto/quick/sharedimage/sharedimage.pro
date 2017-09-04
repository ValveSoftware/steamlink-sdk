CONFIG += testcase
TARGET = tst_sharedimage
CONFIG -= app_bundle

SOURCES += tst_sharedimage.cpp

QT += testlib quick-private

TESTDATA = data/*

OTHER_FILES += \
    data/yellow.png
