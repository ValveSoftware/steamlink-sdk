TEMPLATE = lib
CONFIG += plugin
SOURCES = plugin.cpp
QT = core declarative
TARGET = Plugin
DESTDIR = ../imports/org/qtproject/WrongCase

include(../qmldir_copier.pri)

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
