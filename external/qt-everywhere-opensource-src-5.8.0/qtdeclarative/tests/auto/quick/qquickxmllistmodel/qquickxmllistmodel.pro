CONFIG += testcase
TARGET = tst_qquickxmllistmodel
macx:CONFIG -= app_bundle

SOURCES += tst_qquickxmllistmodel.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private network testlib xmlpatterns

OTHER_FILES += \
    data/groups.qml
