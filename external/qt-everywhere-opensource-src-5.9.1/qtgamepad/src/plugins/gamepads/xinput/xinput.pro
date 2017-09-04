TARGET = xinputgamepad
QT += gamepad gamepad-private

PLUGIN_TYPE = gamepads
PLUGIN_CLASS_NAME = QXInputGamepadBackendPlugin
load(qt_plugin)

HEADERS += qxinputgamepadbackend_p.h
SOURCES += \
    qxinputgamepadbackend.cpp \
    main.cpp

OTHER_FILES += \
    xinput.json
