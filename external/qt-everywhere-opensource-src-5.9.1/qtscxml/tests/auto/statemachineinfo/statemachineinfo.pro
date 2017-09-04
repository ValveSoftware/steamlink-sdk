QT = core gui qml testlib scxml-private
CONFIG += testcase

TARGET = tst_statemachineinfo
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

RESOURCES += tst_statemachineinfo.qrc

SOURCES += \
    tst_statemachineinfo.cpp
