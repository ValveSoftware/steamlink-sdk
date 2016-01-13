CONFIG += testcase
TARGET = tst_qqmldebugservice
macx:CONFIG -= app_bundle

HEADERS += ../shared/qqmldebugtestservice.h

SOURCES += tst_qqmldebugservice.cpp \
           ../shared/qqmldebugtestservice.cpp

INCLUDEPATH += ../shared
include(../../../shared/util.pri)
include(../shared/debugutil.pri)

TESTDATA = data/*

OTHER_FILES += \
    data/test.qml

DEFINES += QT_QML_DEBUG_NO_WARNING

QT += qml-private testlib gui-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
