QT      += opengl widgets
TARGET = view3d

PLUGIN_CLASS_NAME = QView3DPlugin
include(../../plugins.pri)

SOURCES += view3d.cpp view3d_tool.cpp view3d_plugin.cpp
HEADERS += view3d.h view3d_tool.h view3d_plugin.h view3d_global.h
