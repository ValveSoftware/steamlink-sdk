QT += waylandclient-private

OTHER_FILES += \
    qwayland-generic.json

SOURCES += main.cpp

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QWaylandIntegrationPlugin
load(qt_plugin)
