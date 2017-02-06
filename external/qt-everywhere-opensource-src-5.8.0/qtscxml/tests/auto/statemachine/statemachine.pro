QT = core gui qml testlib scxml-private
CONFIG += testcase

TARGET = tst_statemachine
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

RESOURCES += tst_statemachine.qrc

SOURCES += \
    tst_statemachine.cpp
