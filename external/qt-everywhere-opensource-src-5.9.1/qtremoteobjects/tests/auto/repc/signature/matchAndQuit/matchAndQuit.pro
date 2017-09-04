TEMPLATE = app
QT       += remoteobjects core testlib
QT       -= gui

TARGET = matchAndQuit
DESTDIR = ./
CONFIG   += c++11
CONFIG   -= app_bundle

REPC_REPLICA = ../server.rep

SOURCES += main.cpp

INCLUDEPATH += $$PWD
