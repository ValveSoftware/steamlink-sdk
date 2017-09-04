QT = core gui qml testlib scxml
CONFIG += testcase

TARGET = tst_parser
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

RESOURCES += tst_parser.qrc

SOURCES += \
    tst_parser.cpp
