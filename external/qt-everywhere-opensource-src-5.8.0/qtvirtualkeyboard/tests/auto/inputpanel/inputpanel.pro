TEMPLATE = app
TARGET = tst_inputpanel

QT += testlib
CONFIG += qmltestcase console

static {
    QT += svg
    QTPLUGIN += qtvirtualkeyboardplugin
}

SOURCES += $$PWD/tst_inputpanel.cpp

TESTDATA = $$PWD/data/*

OTHER_FILES += \
    $$PWD/data/inputpanel/inputpanel.qml \
    $$PWD/data/inputpanel/handwritinginputpanel.qml \
    $$PWD/data/tst_inputpanel.qml \
