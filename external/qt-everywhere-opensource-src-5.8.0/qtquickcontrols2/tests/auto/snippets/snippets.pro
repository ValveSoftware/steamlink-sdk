TEMPLATE = app
TARGET = tst_snippets

QT += quick testlib
CONFIG += testcase
osx:CONFIG -= app_bundle

DEFINES += QQC2_SNIPPETS_PATH=\\\"$$QQC2_SOURCE_TREE/src/imports/controls/doc/snippets\\\"

SOURCES += \
    $$PWD/tst_snippets.cpp

OTHER_FILES += \
    $$PWD/data/*.qml

TESTDATA += \
    $$PWD/data/*
