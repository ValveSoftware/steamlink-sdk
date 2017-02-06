TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeoroutingmanager

HEADERS += tst_qgeoroutingmanager.h

SOURCES += tst_qgeoroutingmanager.cpp

CONFIG -= app_bundle

QT += location testlib
