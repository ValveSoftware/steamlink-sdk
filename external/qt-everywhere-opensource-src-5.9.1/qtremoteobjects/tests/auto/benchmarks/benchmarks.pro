QT       += network testlib remoteobjects

QT       -= gui

TARGET = tst_benchmarkstest
CONFIG   += console testcase
CONFIG   -= app_bundle

TEMPLATE = app

CONFIG += c++11

OTHER_FILES = ../repfiles/localdatacenter.rep \
              ../repfiles/tcpdatacenter.rep

REPC_SOURCE += $$OTHER_FILES
REPC_REPLICA += $$OTHER_FILES


SOURCES += tst_benchmarkstest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"
