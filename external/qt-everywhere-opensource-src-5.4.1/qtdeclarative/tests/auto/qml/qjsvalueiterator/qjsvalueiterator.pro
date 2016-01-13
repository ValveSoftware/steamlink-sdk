CONFIG += testcase
CONFIG += parallel_test
TARGET = tst_qjsvalueiterator
macx:CONFIG -= app_bundle
QT = core qml testlib
SOURCES  += tst_qjsvalueiterator.cpp


DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
