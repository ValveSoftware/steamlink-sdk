QT += waylandclient-private

OTHER_FILES += \
    bradient.json

SOURCES += main.cpp

QMAKE_USE += wayland-client

PLUGIN_TYPE = wayland-decoration-client
load(qt_plugin)
