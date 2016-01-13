CONFIG += testcase parallel_test
macx:CONFIG -= app_bundle
TARGET = tst_qanimationgroupjob
QT = core-private qml-private testlib
SOURCES = tst_qanimationgroupjob.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
