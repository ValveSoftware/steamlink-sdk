TARGET  = qtscriptdbus

QT = core gui script
CONFIG += qdbus

SOURCES += main.cpp
HEADERS += main.h

PLUGIN_TYPE = script
PLUGIN_CLASS_NAME = QtDBusScriptPlugin
load(qt_plugin)
