CONFIG += testcase
TARGET = tst_dialogs
SOURCES += tst_dialogs.cpp

include (../shared/util.pri)

osx:CONFIG -= app_bundle
osx:CONFIG+=insignificant_test    # QTBUG-30513 - test is unstable
linux-*:CONFIG+=insignificant_test    # QTBUG-30513 - test is unstable
win32:CONFIG+=insignificant_test    # QTBUG-30513 - test is unstable

CONFIG += parallel_test
QT += core-private gui-private qml-private quick-private testlib

TESTDATA = data/*

OTHER_FILES += \
    data/RectWithFileDialog.qml

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
