CONFIG += testcase
TARGET = tst_qqmldebugjs
QT += qml testlib gui-private core-private
CONFIG -= debug_and_release_target
osx:CONFIG -= app_bundle

SOURCES +=     tst_qqmldebugjs.cpp

INCLUDEPATH += ../../shared
include(../../../../shared/util.pri)
include(../../shared/debugutil.pri)

TESTDATA = data/*

OTHER_FILES += data/test.qml data/test.js \
    data/timer.qml \
    data/exception.qml \
    data/oncompleted.qml \
    data/loadjsfile.qml \
    data/condition.qml \
    data/changeBreakpoint.qml \
    data/stepAction.qml \
    data/breakpointRelocation.qml \
    data/createComponent.qml
