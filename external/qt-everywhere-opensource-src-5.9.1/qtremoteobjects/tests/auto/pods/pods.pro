CONFIG += testcase
TARGET = tst_pods
QT += testlib remoteobjects
QT -= gui

SOURCES += tst_pods.cpp

REP_FILES = pods.h

QOBJECT_REP = $$REP_FILES

REPC_REPLICA = $$REP_FILES

contains(QT_CONFIG, c++11): CONFIG += c++11
