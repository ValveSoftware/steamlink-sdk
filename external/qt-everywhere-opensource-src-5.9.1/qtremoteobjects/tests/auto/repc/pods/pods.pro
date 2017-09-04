CONFIG += testcase
TARGET = tst_pods
QT += testlib remoteobjects
QT -= gui

SOURCES += tst_pods.cpp

REP_FILES = pods.rep

#REPC_SOURCE = $$REP_FILES # can't have both source and replica in the same process if PODs are involved...
REPC_REPLICA = $$REP_FILES

contains(QT_CONFIG, c++11):CONFIG += c++11
