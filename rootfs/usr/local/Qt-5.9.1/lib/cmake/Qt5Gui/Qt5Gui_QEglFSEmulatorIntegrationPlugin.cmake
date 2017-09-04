
add_library(Qt5::QEglFSEmulatorIntegrationPlugin MODULE IMPORTED)

_populate_Gui_plugin_properties(QEglFSEmulatorIntegrationPlugin RELEASE "egldeviceintegrations/libqeglfs-emu-integration.so")

list(APPEND Qt5Gui_PLUGINS Qt5::QEglFSEmulatorIntegrationPlugin)
