TARGET = qwbmp

HEADERS += qwbmphandler_p.h
SOURCES += qwbmphandler.cpp
OTHER_FILES += wbmp.json

SOURCES += main.cpp

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QWbmpPlugin
load(qt_plugin)
