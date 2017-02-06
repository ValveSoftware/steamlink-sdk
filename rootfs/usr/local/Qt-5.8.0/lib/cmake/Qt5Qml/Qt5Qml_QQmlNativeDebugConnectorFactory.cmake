
add_library(Qt5::QQmlNativeDebugConnectorFactory MODULE IMPORTED)

_populate_Qml_plugin_properties(QQmlNativeDebugConnectorFactory RELEASE "qmltooling/libqmldbg_native.so")

list(APPEND Qt5Qml_PLUGINS Qt5::QQmlNativeDebugConnectorFactory)
