TARGET = qtmedia_v4lengine
QT += multimedia-private

HEADERS += v4lserviceplugin.h
SOURCES += v4lserviceplugin.cpp

include(radio/radio.pri)

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = V4LServicePlugin
load(qt_plugin)
