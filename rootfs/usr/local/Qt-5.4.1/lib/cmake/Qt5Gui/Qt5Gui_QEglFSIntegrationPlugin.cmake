
add_library(Qt5::QEglFSIntegrationPlugin MODULE IMPORTED)

_populate_Gui_plugin_properties(QEglFSIntegrationPlugin RELEASE "platforms/libqeglfs.so")

list(APPEND Qt5Gui_PLUGINS Qt5::QEglFSIntegrationPlugin)
