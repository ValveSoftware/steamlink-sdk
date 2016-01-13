CONFIG += testcase
TARGET = tst_qdeclarativeparticles

QT += testlib declarative widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativeparticles.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
