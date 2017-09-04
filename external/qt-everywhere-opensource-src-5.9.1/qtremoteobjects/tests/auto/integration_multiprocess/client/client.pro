TEMPLATE = app
QT       += remoteobjects core testlib
QT       -= gui

TARGET = client
DESTDIR = ./
CONFIG   += c++11
CONFIG   -= app_bundle

REPC_REPLICA = ../MyInterface.rep

SOURCES += main.cpp \

HEADERS += \

INCLUDEPATH += $$PWD
