TEMPLATE = app
TARGET = tst_objectcount

QT += quick testlib core-private
CONFIG += benchmark
osx:CONFIG -= app_bundle

SOURCES += \
    tst_objectcount.cpp
