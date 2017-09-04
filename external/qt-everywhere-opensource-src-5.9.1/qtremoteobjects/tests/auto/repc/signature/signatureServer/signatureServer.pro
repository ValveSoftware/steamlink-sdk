TEMPLATE = app
QT       += remoteobjects core testlib
QT       -= gui

TARGET = signatureServer
DESTDIR = ./
CONFIG   += c++11
CONFIG   -= app_bundle

REPC_SOURCE = ../server.rep

SOURCES += main.cpp

HEADERS += $$OUT_PWD/rep_server_source.h

INCLUDEPATH += $$PWD
