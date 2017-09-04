TARGET  = qsvgicon

HEADERS += qsvgiconengine.h
SOURCES += main.cpp \
           qsvgiconengine.cpp
OTHER_FILES += qsvgiconengine.json
OTHER_FILES += qsvgiconengine-nocompress.json
QT += svg core-private gui-private

PLUGIN_TYPE = iconengines
PLUGIN_EXTENDS = svg
PLUGIN_CLASS_NAME = QSvgIconPlugin
load(qt_plugin)
