TARGET = qtposition_testplugin
QT += positioning

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = TestPositionPlugin
PLUGIN_EXTENDS = -
load(qt_plugin)

SOURCES += plugin.cpp

OTHER_FILES += \
    plugin.json
