TEMPLATE = lib
CONFIG += plugin
SOURCES = simpleplugin.cpp
OTHER_FILES += simpleplugin.json
QT = core script
TARGET = simpleplugin
DESTDIR = ../plugins/script
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
