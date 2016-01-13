CONFIG += testcase
testcase.timeout = 600 # this test is slow
TARGET = tst_qmlvisual

QT += testlib declarative gui widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qmlvisual.cpp

DEFINES += QT_TEST_SOURCE_DIR=\"\\\"$$PWD\\\"\"

CONFIG += parallel_test

CONFIG+=insignificant_test # QTBUG-26705
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
