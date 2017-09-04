CONFIG += testcase c++11
CONFIG -= app_bundle

TARGET = tst_signature

DESTDIR = ./

QT += testlib remoteobjects
QT -= gui

SOURCES += tst_signature.cpp
