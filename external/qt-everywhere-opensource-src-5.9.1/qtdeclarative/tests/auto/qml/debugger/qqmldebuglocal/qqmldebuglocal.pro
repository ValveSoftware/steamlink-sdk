CONFIG += testcase
TARGET = tst_qqmldebuglocal
osx:CONFIG -= app_bundle

HEADERS += ../shared/qqmldebugtestservice.h

SOURCES += tst_qqmldebuglocal.cpp \
           ../shared/qqmldebugtestservice.cpp

INCLUDEPATH += ../shared
include(../shared/debugutil.pri)

QT += qml-private testlib gui-private core-private

DEFINES += QT_QML_DEBUG_NO_WARNING
