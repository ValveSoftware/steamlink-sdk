TARGET = qtcanbustestgenericv1

QT = core serialbus

HEADERS += \
    dummybackendv1.h

SOURCES += \
    main.cpp \
    dummybackendv1.cpp

DISTFILES = plugin.json

PLUGIN_TYPE = canbus
PLUGIN_EXTENDS = -
PLUGIN_CLASS_NAME = GenericBusPluginV1
load(qt_plugin)
