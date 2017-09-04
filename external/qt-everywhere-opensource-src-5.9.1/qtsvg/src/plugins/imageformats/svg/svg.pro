TARGET  = qsvg

HEADERS += qsvgiohandler.h
SOURCES += main.cpp \
           qsvgiohandler.cpp
QT += svg

PLUGIN_TYPE = imageformats
PLUGIN_EXTENDS = svg
PLUGIN_CLASS_NAME = QSvgPlugin
load(qt_plugin)
