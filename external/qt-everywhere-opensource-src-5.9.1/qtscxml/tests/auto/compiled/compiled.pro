QT = core gui qml testlib scxml
CONFIG += testcase

TARGET = tst_compiled
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    tst_compiled.cpp

STATECHARTS = \
    ids1.scxml \
    eventnames1.scxml \
    eventnames2.scxml \
    statemachineunicodename.scxml \
    anonymousstate.scxml \
    submachineunicodename.scxml \
    datainnulldatamodel.scxml \
    initialhistory.scxml \
    connection.scxml \
    topmachine.scxml

RESOURCES = tst_compiled.qrc
