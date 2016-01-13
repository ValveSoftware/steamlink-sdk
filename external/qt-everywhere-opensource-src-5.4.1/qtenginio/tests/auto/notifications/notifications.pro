QT       += testlib enginio enginio-private
QT       -= gui

TARGET = tst_notifications
CONFIG   += console testcase
CONFIG   -= app_bundle

TEMPLATE = app

include(../common/common.pri)

SOURCES += tst_notifications.cpp
