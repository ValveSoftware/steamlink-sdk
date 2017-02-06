
add_library(Qt5::QQmlDebugServerFactory MODULE IMPORTED)

_populate_Qml_plugin_properties(QQmlDebugServerFactory RELEASE "qmltooling/libqmldbg_server.so")

list(APPEND Qt5Qml_PLUGINS Qt5::QQmlDebugServerFactory)
