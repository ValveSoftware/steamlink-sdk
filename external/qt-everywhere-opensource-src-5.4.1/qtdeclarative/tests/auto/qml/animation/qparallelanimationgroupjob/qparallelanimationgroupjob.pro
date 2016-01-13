CONFIG += testcase
CONFIG += parallel_test
macx:CONFIG -= app_bundle
TARGET = tst_qparallelanimationgroupjob
QT = core-private gui qml-private testlib gui-private
SOURCES = tst_qparallelanimationgroupjob.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
