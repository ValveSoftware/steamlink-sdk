CONFIG += testcase
TARGET = tst_qqmlprofilerservice
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlprofilerservice.cpp

INCLUDEPATH += ../shared
include(../../../shared/util.pri)
include(../shared/debugutil.pri)

TESTDATA = data/*

QT += core qml testlib gui-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

OTHER_FILES += \
    data/pixmapCacheTest.qml \
    data/controlFromJS.qml \
    data/test.qml \
    data/exit.qml \
    data/scenegraphTest.qml \
    data/TestImage_2x2.png \
    data/signalSourceLocation.qml \
    data/javascript.qml
