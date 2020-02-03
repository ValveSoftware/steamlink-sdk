
add_library(Qt5::QEglFSVivIntegrationPlugin MODULE IMPORTED)


_populate_Gui_plugin_properties(QEglFSVivIntegrationPlugin RELEASE "egldeviceintegrations/libqeglfs-viv-integration.so" FALSE)

list(APPEND Qt5Gui_PLUGINS Qt5::QEglFSVivIntegrationPlugin)
set_property(TARGET Qt5::Gui APPEND PROPERTY QT_ALL_PLUGINS_egldeviceintegrations Qt5::QEglFSVivIntegrationPlugin)
set_property(TARGET Qt5::QEglFSVivIntegrationPlugin PROPERTY QT_PLUGIN_TYPE "egldeviceintegrations")
set_property(TARGET Qt5::QEglFSVivIntegrationPlugin PROPERTY QT_PLUGIN_EXTENDS "")
