CONFIG += testcase
TARGET = tst_qquickmaterialstyleconf
SOURCES += tst_qquickmaterialstyleconf.cpp

macos:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private testlib quicktemplates2-private quickcontrols2-private

include (../shared/util.pri)

RESOURCES += qquickmaterialstyleconf.qrc

TESTDATA = data/*

OTHER_FILES += \
    data/*.qml

