CONFIG += testcase
TARGET = tst_parser
QT += testlib core-private
QT -= gui

CONFIG += qlalr
QLALRSOURCES += $$QTRO_SOURCE_TREE/src/repparser/parser.g
INCLUDEPATH += $$QTRO_SOURCE_TREE/src/repparser
win32-msvc*:!wince: QMAKE_CXXFLAGS += /wd4129

SOURCES += $$PWD/tst_parser.cpp

contains(QT_CONFIG, c++11):CONFIG += c++11
