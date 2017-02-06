TARGET = qtvectorcanbus

QT = core-private serialbus

HEADERS += \
    vectorcanbackend.h \
    vectorcanbackend_p.h \
    vectorcan_symbols_p.h

SOURCES += \
    main.cpp \
    vectorcanbackend.cpp

DISTFILES = plugin.json

PLUGIN_TYPE = canbus
PLUGIN_EXTENDS = serialbus
PLUGIN_CLASS_NAME = VectorCanBusPlugin
load(qt_plugin)
