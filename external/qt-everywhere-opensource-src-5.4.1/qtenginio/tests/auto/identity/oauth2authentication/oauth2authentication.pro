QT       += testlib enginio
QT       -= gui

TARGET = tst_oauth2authentication
CONFIG   += console testcase
CONFIG   -= app_bundle

TEMPLATE = app

include(../common/common.pri)

SOURCES += tst_oauth2authentication.cpp