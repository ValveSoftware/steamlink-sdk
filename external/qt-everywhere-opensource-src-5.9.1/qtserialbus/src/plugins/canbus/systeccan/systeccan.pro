TARGET = qtsysteccanbus

QT = core serialbus

HEADERS += \
    systeccanbackend.h \
    systeccanbackend_p.h \
    systeccan_symbols_p.h

SOURCES += \
    main.cpp \
    systeccanbackend.cpp

DISTFILES = plugin.json

PLUGIN_TYPE = canbus
PLUGIN_EXTENDS = serialbus
PLUGIN_CLASS_NAME = SystecCanBusPlugin
load(qt_plugin)
