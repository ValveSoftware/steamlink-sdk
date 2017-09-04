CONFIG += testcase
TARGET = tst_qquickxmllistmodel
macx:CONFIG -= app_bundle

SOURCES += tst_qquickxmllistmodel.cpp \
           ../../../../src/imports/xmllistmodel/qqmlxmllistmodel.cpp
HEADERS +=  ../../../../src/imports/xmllistmodel/qqmlxmllistmodel_p.h

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private  qml-private network testlib xmlpatterns

OTHER_FILES += \
    data/groups.qml
