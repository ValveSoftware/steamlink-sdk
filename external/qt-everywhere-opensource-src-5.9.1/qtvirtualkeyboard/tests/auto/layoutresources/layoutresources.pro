CONFIG += testcase
TARGET = tst_layoutresources

macos:CONFIG -= app_bundle

QT += testlib qml quick

static {
    QT += svg
    QTPLUGIN += qtvirtualkeyboardplugin
}

include(../shared/layouttest.pri)

SOURCES += $$PWD/tst_layoutresources.cpp

RESOURCES += \
    $$PWD/data/resourceslayout.qrc

DEFINES += SRC_DIR=\\\"$$PWD\\\"
