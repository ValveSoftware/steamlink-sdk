TEMPLATE = app
CONFIG += testcase
TARGET = tst_qgeocircle

SOURCES += \
    tst_qgeocircle.cpp

QT += positioning testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
