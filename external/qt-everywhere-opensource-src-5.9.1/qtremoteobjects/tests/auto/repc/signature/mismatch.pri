TEMPLATE = app
QT       += remoteobjects core testlib
QT       -= gui

DESTDIR = ./
CONFIG   += c++11
CONFIG   -= app_bundle

INCLUDEPATH += $$PWD
SOURCES += $$PWD/mismatch.cpp
