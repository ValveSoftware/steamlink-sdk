TEMPLATE = lib
TARGET = Slow
INCLUDEPATH += .
QT += quick qml

# Input
HEADERS += plugin.h slow.h
SOURCES += plugin.cpp slow.cpp

DESTDIR = ../Slow
IMPORT_FILES = qmldir
include (../../../shared/imports.pri)
