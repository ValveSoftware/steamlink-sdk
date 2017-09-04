CONFIG += testcase
TARGET = tst_layoutfilesystem

macos:CONFIG -= app_bundle

QT += testlib qml quick

static {
    QT += svg
    QTPLUGIN += qtvirtualkeyboardplugin
}

include(../shared/layouttest.pri)

SOURCES += $$PWD/tst_layoutfilesystem.cpp

OTHER_FILES += \
    data/layouts/en_GB/main.qml \

DEFINES += SRC_DIR=\\\"$$PWD\\\"
