QT       += testlib enginio
QT       -= gui

TARGET = tst_enginioclient
CONFIG   += console testcase
CONFIG   -= app_bundle

TEMPLATE = app

include(../common/common.pri)

SOURCES += tst_enginioclient.cpp