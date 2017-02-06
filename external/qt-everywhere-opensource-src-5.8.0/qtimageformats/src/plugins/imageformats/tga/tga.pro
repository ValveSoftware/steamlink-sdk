TARGET = qtga

HEADERS += qtgahandler.h \
    qtgafile.h
SOURCES += qtgahandler.cpp \
    qtgafile.cpp
OTHER_FILES += tga.json

SOURCES += main.cpp

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QTgaPlugin
load(qt_plugin)
