CONFIG += testcase
TARGET = tst_enums
QT += testlib remoteobjects
QT -= gui

SOURCES += tst_enums.cpp

REP_FILES = enums.rep

REPC_REPLICA = $$REP_FILES

contains(QT_CONFIG, c++11): CONFIG += c++11
