option(host_build)

QT += testlib scxml
CONFIG += testcase

QT += core qml
QT -= gui

TARGET = tst_cppgen
CONFIG += console c++11
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    tst_cppgen.cpp

load(qt_tool)
