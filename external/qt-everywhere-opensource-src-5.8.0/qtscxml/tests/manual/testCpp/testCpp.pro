option(host_build)

QT       += core qml scxml
QT       -= gui

TARGET = testCpp
CONFIG   += console c++11
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    testcpp.cpp

OTHER_FILES += genTestSxcml.py out.scxml
STATECHARTS = out.scxml

load(qt_tool)
load(qscxmlcpp)
