QT = core gui qml testlib scxml
CONFIG += testcase

TARGET = tst_dynamicmetaobject
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

RESOURCES += tst_dynamicmetaobject.qrc

SOURCES += \
    tst_dynamicmetaobject.cpp
