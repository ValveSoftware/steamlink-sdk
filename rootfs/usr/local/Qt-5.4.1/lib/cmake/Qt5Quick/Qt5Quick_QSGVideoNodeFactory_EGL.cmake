
add_library(Qt5::QSGVideoNodeFactory_EGL MODULE IMPORTED)

_populate_Quick_plugin_properties(QSGVideoNodeFactory_EGL RELEASE "video/videonode/libeglvideonode.so")

list(APPEND Qt5Quick_PLUGINS Qt5::QSGVideoNodeFactory_EGL)
