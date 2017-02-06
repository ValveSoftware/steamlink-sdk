TARGET = sdl2gamepad
QT += gamepad gamepad-private

PLUGIN_TYPE = gamepads
PLUGIN_CLASS_NAME = QSdl2GamepadBackendPlugin
load(qt_plugin)

QMAKE_USE += sdl2

HEADERS += qsdlgamepadbackend_p.h
SOURCES += \
    qsdlgamepadbackend.cpp \
    main.cpp

OTHER_FILES += \
    sdl2.json

