QT += waylandclient-private

OTHER_FILES += \
    qwayland-generic.json

SOURCES += main.cpp

PLUGIN_TYPE = platforms
load(qt_plugin)
