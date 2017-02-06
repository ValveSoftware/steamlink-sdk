QT = core quick webchannel-private

INCLUDEPATH += ../../webchannel
VPATH += ../../webchannel

IMPORT_VERSION = 1.0

SOURCES += \
    plugin.cpp

load(qml_plugin)
