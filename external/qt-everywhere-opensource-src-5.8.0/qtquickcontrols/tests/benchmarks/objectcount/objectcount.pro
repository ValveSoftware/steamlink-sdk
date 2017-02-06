TEMPLATE = app
TARGET = tst_objectcount

QT += quick testlib core-private
CONFIG += testcase
osx:CONFIG -= app_bundle

SOURCES += \
    tst_objectcount.cpp
