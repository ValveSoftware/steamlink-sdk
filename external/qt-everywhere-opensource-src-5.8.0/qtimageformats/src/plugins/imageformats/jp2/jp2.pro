TARGET  = qjp2

include(qjp2handler.pri)
SOURCES += main.cpp
OTHER_FILES += jp2.json

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QJp2Plugin
load(qt_plugin)
