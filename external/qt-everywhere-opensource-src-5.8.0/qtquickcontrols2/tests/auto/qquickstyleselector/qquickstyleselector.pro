CONFIG += testcase
TARGET = tst_qquickstyleselector
SOURCES += tst_qquickstyleselector.cpp

osx:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private quickcontrols2-private testlib

include (../shared/util.pri)

resourcestyle.prefix = /
resourcestyle.files += $$PWD/ResourceStyle/Button.qml
RESOURCES += resourcestyle

TESTDATA = data/*

OTHER_FILES += \
    data/*.qml

