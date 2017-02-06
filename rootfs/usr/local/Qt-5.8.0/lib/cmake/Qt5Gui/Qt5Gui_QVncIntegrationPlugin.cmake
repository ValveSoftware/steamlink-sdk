
add_library(Qt5::QVncIntegrationPlugin MODULE IMPORTED)

_populate_Gui_plugin_properties(QVncIntegrationPlugin RELEASE "platforms/libqvnc.so")

list(APPEND Qt5Gui_PLUGINS Qt5::QVncIntegrationPlugin)
