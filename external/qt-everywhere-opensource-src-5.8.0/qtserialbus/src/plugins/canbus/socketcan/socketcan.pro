TARGET = qtsocketcanbus

QT = core-private serialbus

HEADERS += \
    socketcanbackend.h

SOURCES += \
    main.cpp \
    socketcanbackend.cpp

DISTFILES = plugin.json

PLUGIN_TYPE = canbus
PLUGIN_EXTENDS = serialbus
PLUGIN_CLASS_NAME = SocketCanBusPlugin
load(qt_plugin)
