TARGET = evdevgamepad
QT += core-private devicediscovery_support-private gamepad gamepad-private

PLUGIN_TYPE = gamepads
PLUGIN_CLASS_NAME = QEvdevGamepadBackendPlugin
load(qt_plugin)

HEADERS += qevdevgamepadbackend_p.h
SOURCES += \
    qevdevgamepadbackend.cpp \
    main.cpp

OTHER_FILES += \
    evdev.json
