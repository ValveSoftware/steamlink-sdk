CONFIG += testcase
TARGET = tst_qqmlitemmodels
macx:CONFIG -= app_bundle

HEADERS = qtestmodel.h testtypes.h
SOURCES += tst_qqmlitemmodels.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core qml testlib

DISTFILES += \
    data/modelindex.qml \
    data/persistentmodelindex.qml \
    data/itemselectionrange.qml \
    data/modelindexlist.qml \
    data/itemselection.qml \
    data/modelindexconversion.qml
