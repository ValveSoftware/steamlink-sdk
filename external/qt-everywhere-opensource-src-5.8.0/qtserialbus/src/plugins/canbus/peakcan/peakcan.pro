TARGET = qtpeakcanbus

QT = core-private serialbus

HEADERS += \
    peakcanbackend.h \
    peakcanbackend_p.h \
    peakcan_symbols_p.h

SOURCES += \
    main.cpp \
    peakcanbackend.cpp

DISTFILES = plugin.json

PLUGIN_TYPE = canbus
PLUGIN_EXTENDS = serialbus
PLUGIN_CLASS_NAME = PeakCanBusPlugin
load(qt_plugin)
