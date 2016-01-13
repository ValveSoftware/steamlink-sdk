CONFIG += testcase
TARGET = tst_qqmldebugclient
macx:CONFIG -= app_bundle

HEADERS += ../shared/qqmldebugtestservice.h

SOURCES += tst_qqmldebugclient.cpp \
           ../shared/qqmldebugtestservice.cpp

INCLUDEPATH += ../shared
include(../shared/debugutil.pri)

DEFINES += QT_QML_DEBUG_NO_WARNING

CONFIG += qml_debug

QT += qml-private testlib gui-private
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
