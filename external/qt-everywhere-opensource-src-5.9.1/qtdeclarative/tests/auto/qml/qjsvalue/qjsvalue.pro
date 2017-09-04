CONFIG += testcase
TARGET = tst_qjsvalue
macx:CONFIG -= app_bundle
QT += qml widgets testlib gui-private
SOURCES  += tst_qjsvalue.cpp
HEADERS  += tst_qjsvalue.h
