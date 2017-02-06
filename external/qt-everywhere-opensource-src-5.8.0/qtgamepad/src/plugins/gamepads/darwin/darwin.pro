TARGET = darwingamepad
QT += gamepad gamepad-private

PLUGIN_TYPE = gamepads
PLUGIN_EXTENDS = gamepad
PLUGIN_CLASS_NAME = QDarwinGamepadBackendPlugin
load(qt_plugin)

LIBS += -framework GameController -framework Foundation

HEADERS += qdarwingamepadbackend_p.h
OBJECTIVE_SOURCES += \
    qdarwingamepadbackend.mm

SOURCES += \
    main.cpp

OTHER_FILES += \
    darwin.json
