TEMPLATE = app
TARGET = tst_styles

QT += testlib
CONFIG += qmltestcase console

static {
    QT += svg
    QTPLUGIN += qtvirtualkeyboardplugin
}

SOURCES += $$PWD/tst_styles.cpp

TESTDATA = $$PWD/data/*
DEFINES += TESTDATA_DIR=\\\"$$_PRO_FILE_PWD_/data\\\"

OTHER_FILES += \
    $$PWD/data/tst_styles.qml \

DISTFILES += \
    data/tst_styles.qml \
    data/QtQuick/VirtualKeyboard/Styles/test/style.qml \

