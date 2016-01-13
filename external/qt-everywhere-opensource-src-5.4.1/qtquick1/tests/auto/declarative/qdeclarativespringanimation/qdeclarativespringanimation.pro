CONFIG += testcase
TARGET = tst_qdeclarativespringanimation

QT += testlib declarative declarative-private gui core-private script-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativespringanimation.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
