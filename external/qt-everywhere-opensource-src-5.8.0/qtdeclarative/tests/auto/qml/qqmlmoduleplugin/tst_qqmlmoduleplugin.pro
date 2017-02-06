CONFIG += testcase
TARGET = tst_qqmlmoduleplugin

HEADERS = ../../shared/testhttpserver.h
SOURCES = tst_qqmlmoduleplugin.cpp \
          ../../shared/testhttpserver.cpp
CONFIG -= app_bundle

include (../../shared/util.pri)

TESTDATA = data/* imports/* $$OUT_PWD/imports/*

QT += core-private gui-private qml-private network testlib
