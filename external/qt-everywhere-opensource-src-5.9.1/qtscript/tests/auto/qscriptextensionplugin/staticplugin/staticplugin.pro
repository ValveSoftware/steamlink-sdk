TEMPLATE = lib
CONFIG += static plugin
CONFIG -= create_prl    # not needed, and complicates debug/release
SOURCES = staticplugin.cpp
RESOURCES = staticplugin.qrc
QT = core script
DEFINES += QT_STATICPLUGIN
TARGET = staticplugin
DESTDIR = ../plugins/script
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
