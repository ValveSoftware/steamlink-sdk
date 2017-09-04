CONFIG += testcase
TARGET = tst_dynamicnodecreation
osx:CONFIG -= app_bundle

INCLUDEPATH += ../../shared/
SOURCES += \
    tst_dynamicnodecreation.cpp

OTHER_FILES = \
    data/createDynamicChild.qml \
    data/createMultiple.qml \
    data/createSingle.qml \
    data/inactive.qml \
    data/dynamicEntity.qml

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private 3dcore 3dcore-private 3dquick 3dquick-private testlib
