CONFIG += testcase
TARGET = tst_qquickxmllistmodel
macx:CONFIG -= app_bundle

SOURCES += tst_qquickxmllistmodel.cpp

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test

QT += core-private gui-private  qml-private network testlib xmlpatterns
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

OTHER_FILES += \
    data/groups.qml
