TEMPLATE = app
TARGET = tst_creationtime

QT += qml testlib
CONFIG += testcase
osx:CONFIG -= app_bundle

DEFINES += QQC2_IMPORT_PATH=\\\"$$QQC2_SOURCE_TREE/src/imports\\\"

SOURCES += \
    tst_creationtime.cpp
