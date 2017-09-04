CONFIG += testcase c++11
CONFIG -= app_bundle
TARGET = tst_integration_multiprocess
DESTDIR = ./
QT += testlib remoteobjects
QT -= gui

SOURCES += tst_integration_multiprocess.cpp
