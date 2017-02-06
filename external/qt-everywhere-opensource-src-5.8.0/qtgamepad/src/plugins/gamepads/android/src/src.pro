TARGET = androidgamepad
QT += core-private gamepad gamepad-private

PLUGIN_TYPE = gamepads
PLUGIN_CLASS_NAME = QAndroidGamepadBackendPlugin

load(qt_plugin)

HEADERS += \
    qandroidgamepadbackend_p.h

SOURCES += \
    main.cpp \
    qandroidgamepadbackend.cpp

DISTFILES += \
    android.json
