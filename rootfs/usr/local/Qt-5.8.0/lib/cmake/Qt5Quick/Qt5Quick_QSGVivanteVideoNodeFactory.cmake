
add_library(Qt5::QSGVivanteVideoNodeFactory MODULE IMPORTED)

_populate_Quick_plugin_properties(QSGVivanteVideoNodeFactory RELEASE "video/videonode/libimx6vivantevideonode.so")

list(APPEND Qt5Quick_PLUGINS Qt5::QSGVivanteVideoNodeFactory)
