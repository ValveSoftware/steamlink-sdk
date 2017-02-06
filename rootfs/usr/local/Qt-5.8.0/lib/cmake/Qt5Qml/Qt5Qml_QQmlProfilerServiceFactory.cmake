
add_library(Qt5::QQmlProfilerServiceFactory MODULE IMPORTED)

_populate_Qml_plugin_properties(QQmlProfilerServiceFactory RELEASE "qmltooling/libqmldbg_profiler.so")

list(APPEND Qt5Qml_PLUGINS Qt5::QQmlProfilerServiceFactory)
