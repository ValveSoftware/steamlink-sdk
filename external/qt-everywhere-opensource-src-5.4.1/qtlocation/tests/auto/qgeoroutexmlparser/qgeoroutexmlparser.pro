CONFIG += testcase
TARGET = tst_qgeoroutexmlparser

plugin.path = ../../../src/plugins/geoservices/nokia/

SOURCES += tst_qgeoroutexmlparser.cpp \
           $$plugin.path/qgeoroutexmlparser.cpp
HEADERS += $$plugin.path/qgeoroutexmlparser.h
INCLUDEPATH += $$plugin.path
RESOURCES += fixtures.qrc

QT += location testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
