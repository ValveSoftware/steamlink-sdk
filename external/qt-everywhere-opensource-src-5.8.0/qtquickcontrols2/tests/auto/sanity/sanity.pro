TEMPLATE = app
TARGET = tst_sanity

QT += qml testlib core-private qml-private
CONFIG += testcase
osx:CONFIG -= app_bundle

DEFINES += QQC2_IMPORT_PATH=\\\"$$QQC2_SOURCE_TREE/src/imports\\\"

SOURCES += \
    $$PWD/tst_sanity.cpp

OTHER_FILES += \
    $$PWD/BLACKLIST
