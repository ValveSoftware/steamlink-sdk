TARGET  = qmng

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QMngPlugin
load(qt_plugin)

include(qmnghandler.pri)
SOURCES += main.cpp
OTHER_FILES += mng.json
