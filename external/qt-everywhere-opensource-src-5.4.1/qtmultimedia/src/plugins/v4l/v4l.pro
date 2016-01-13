TARGET = qtmedia_v4lengine
QT += multimedia-private

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = V4LServicePlugin
load(qt_plugin)

HEADERS += v4lserviceplugin.h
SOURCES += v4lserviceplugin.cpp

include(radio/radio.pri)
