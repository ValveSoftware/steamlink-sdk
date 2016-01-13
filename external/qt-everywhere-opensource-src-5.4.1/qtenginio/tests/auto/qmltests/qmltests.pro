TEMPLATE = app

include(../common/common.pri)
SOURCES += tst_qmltest.cpp

TARGET = tst_qmltests

QT += enginio
CONFIG += testcase
CONFIG -= app_bundle

QT += qmltest testlib

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$_PRO_FILE_PWD_\\\"

OTHER_FILES += \
    tst_enginioclient.qml \
    tst_enginioreply.qml \
    tst_files.qml \
    tst_identity.qml \
    tst_query.qml \
    tst_model.qml

