TEMPLATE = app
TARGET = tst_pressandhold

QT += quick testlib
CONFIG += testcase
osx:CONFIG -= app_bundle

SOURCES += \
    $$PWD/tst_pressandhold.cpp
