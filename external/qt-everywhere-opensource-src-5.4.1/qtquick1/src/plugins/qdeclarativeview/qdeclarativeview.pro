TARGET      = qdeclarativeview
QT         += declarative widgets designer

PLUGIN_TYPE = designer
PLUGIN_CLASS_NAME = QDeclarativeViewPlugin
CONFIG += tool_plugin
load(qt_plugin)

SOURCES += qdeclarativeview_plugin.cpp
HEADERS += qdeclarativeview_plugin.h
OTHER_FILES += qdeclarativeview.json
