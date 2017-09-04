CONFIG += testcase
TARGET = tst_qquickwidget
SOURCES += tst_qquickwidget.cpp

include (../../shared/util.pri)

osx:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private quickwidgets quickwidgets-private widgets-private testlib

TESTDATA = data/*

OTHER_FILES += \
    animating.qml  \
    error1.qml  \
    rectangle.qml  \
    resizemodeitem.qml

