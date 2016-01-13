CONFIG += testcase
testcase.timeout = 400 # this test is slow
TARGET = tst_examples

!qtHaveModule(webkit): DEFINES += QT_NO_WEBKIT
!qtHaveModule(xmlpatterns): DEFINES += QT_NO_XMLPATTERNS

QT += testlib declarative
macx:CONFIG -= app_bundle

SOURCES += tst_examples.cpp

include(../../../../tools/qml/qml.pri)

DEFINES += SRCDIR=\\\"$$PWD\\\"

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
