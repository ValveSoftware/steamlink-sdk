CONFIG += testcase
TARGET = tst_qnodeinstantiator
osx:CONFIG -= app_bundle

INCLUDEPATH += ../../shared/
SOURCES += \
    tst_quick3dnodeinstantiator.cpp
HEADERS += stringmodel.h

OTHER_FILES = \
    data/createAndRemove.qml \
    data/createMultiple.qml \
    data/createNone.qml \
    data/createSingle.qml \
    data/createNested.qml \
    data/inactive.qml \
    data/stringModel.qml

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private 3dcore 3dquick 3dquick-private testlib
