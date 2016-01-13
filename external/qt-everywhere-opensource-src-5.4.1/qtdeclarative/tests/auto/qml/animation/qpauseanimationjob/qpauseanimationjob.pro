CONFIG += testcase
CONFIG += parallel_test
macx:CONFIG -= app_bundle
TARGET = tst_qpauseanimationjob
QT = core-private gui-private qml-private testlib
SOURCES = tst_qpauseanimationjob.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
