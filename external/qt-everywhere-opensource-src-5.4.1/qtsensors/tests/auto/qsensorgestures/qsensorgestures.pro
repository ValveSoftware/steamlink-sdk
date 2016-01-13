TEMPLATE = app
TARGET = tst_qsensorgesturetest
CONFIG += testcase
DEFINES += QT_STATICPLUGIN

QT += core testlib sensors
QT -= gui

SOURCES += tst_qsensorgesturetest.cpp


PLUGIN_1_HEADERS = \
              plugins/test1/qtestsensorgestureplugindup.h \
              plugins/test1/qtestrecognizerdup.h \
              plugins/test1/qtest2recognizerdup.h

PLUGIN_1_SOURCES = \
               plugins/test1/qtestsensorgestureplugindup.cpp \
               plugins/test1/qtestrecognizerdup.cpp \
               plugins/test1/qtest2recognizerdup.cpp

HEADERS += $$PLUGIN_1_HEADERS
SOURCES += $$PLUGIN_1_SOURCES

HEADERS +=  \
               plugins/test/qtestsensorgestureplugin_p.h \
               plugins/test/qtestrecognizer.h \
               plugins/test/qtest2recognizer.h

SOURCES += \
               plugins/test/qtestsensorgestureplugin.cpp \
               plugins/test/qtestrecognizer.cpp \
               plugins/test/qtest2recognizer.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
