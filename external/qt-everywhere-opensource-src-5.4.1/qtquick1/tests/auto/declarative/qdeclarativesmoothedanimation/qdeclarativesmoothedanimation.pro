CONFIG += testcase
TARGET = tst_qdeclarativesmoothedanimation

QT += testlib declarative declarative-private gui core-private script-private widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativesmoothedanimation.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
