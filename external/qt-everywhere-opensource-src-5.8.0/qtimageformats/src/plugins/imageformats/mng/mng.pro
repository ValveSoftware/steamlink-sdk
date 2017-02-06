TARGET  = qmng

include(qmnghandler.pri)
SOURCES += main.cpp
OTHER_FILES += mng.json

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QMngPlugin
load(qt_plugin)
