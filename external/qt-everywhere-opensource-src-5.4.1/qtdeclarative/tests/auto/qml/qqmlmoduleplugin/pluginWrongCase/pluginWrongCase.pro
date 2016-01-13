TEMPLATE = lib
CONFIG += plugin
SOURCES = plugin.cpp
QT = core qml
TARGET = Plugin
DESTDIR = ../imports/org/qtproject/WrongCase

QT += core-private gui-private qml-private

IMPORT_FILES = \
        qmldir

include (../../../shared/imports.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
