
add_library(Qt5::QEglFSVivIntegrationPlugin MODULE IMPORTED)

_populate_Gui_plugin_properties(QEglFSVivIntegrationPlugin RELEASE "egldeviceintegrations/libqeglfs-viv-integration.so")

list(APPEND Qt5Gui_PLUGINS Qt5::QEglFSVivIntegrationPlugin)
