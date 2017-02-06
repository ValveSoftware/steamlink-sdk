TARGET = qtcanbustestgeneric

QT = core serialbus

HEADERS += \
    dummybackend.h

SOURCES += \
    main.cpp \
    dummybackend.cpp

DISTFILES = plugin.json

PLUGIN_TYPE = canbus
PLUGIN_EXTENDS = -
PLUGIN_CLASS_NAME = GenericBusPlugin
load(qt_plugin)
