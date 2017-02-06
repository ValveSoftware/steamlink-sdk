TEMPLATE = app
TARGET = tst_objectcount

QT += quick testlib core-private
CONFIG += testcase
osx:CONFIG -= app_bundle

DEFINES += QQC2_IMPORT_PATH=\\\"$$QQC2_SOURCE_TREE/src/imports\\\"

SOURCES += \
    tst_objectcount.cpp
