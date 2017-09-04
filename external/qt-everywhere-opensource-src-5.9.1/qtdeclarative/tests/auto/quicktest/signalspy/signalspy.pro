CONFIG += testcase
TARGET = tst_signalspy
macos:CONFIG -= app_bundle

SOURCES += tst_signalspy.cpp mypropertymap.cpp
HEADERS += mypropertymap.h
QT += quick testlib

include (../../shared/util.pri)
