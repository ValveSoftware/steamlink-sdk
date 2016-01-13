TEMPLATE = app

QT += declarative declarative-private gui widgets
CONFIG += declarative_debug

CONFIG += console
CONFIG -= app_bundle

DESTDIR = ./

INSTALLS =

SOURCES += main.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
