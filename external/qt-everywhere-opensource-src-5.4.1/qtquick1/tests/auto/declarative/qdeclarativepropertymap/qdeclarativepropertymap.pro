CONFIG += testcase
TARGET = tst_qdeclarativepropertymap

QT += testlib declarative declarative-private
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativepropertymap.cpp

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
