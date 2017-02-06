
add_library(Qt5::QEvdevGamepadBackendPlugin MODULE IMPORTED)

_populate_Gamepad_plugin_properties(QEvdevGamepadBackendPlugin RELEASE "gamepads/libevdevgamepad.so")

list(APPEND Qt5Gamepad_PLUGINS Qt5::QEvdevGamepadBackendPlugin)
