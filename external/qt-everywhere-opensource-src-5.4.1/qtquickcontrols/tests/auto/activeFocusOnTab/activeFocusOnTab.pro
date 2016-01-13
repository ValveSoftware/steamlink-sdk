CONFIG += testcase
TARGET = tst_activeFocusOnTab
macx:CONFIG -= app_bundle

SOURCES += tst_activeFocusOnTab.cpp

include (../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private testlib
!no_desktop: QT += widgets
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
