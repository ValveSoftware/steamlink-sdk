
add_library(Qt5::QLocalClientConnectionFactory MODULE IMPORTED)

_populate_Qml_plugin_properties(QLocalClientConnectionFactory RELEASE "qmltooling/libqmldbg_local.so")

list(APPEND Qt5Qml_PLUGINS Qt5::QLocalClientConnectionFactory)
