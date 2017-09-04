TEMPLATE = app
TARGET = tst_pressandhold

QT += quick testlib
CONFIG += testcase
macos:CONFIG -= app_bundle

SOURCES += \
    $$PWD/tst_pressandhold.cpp
