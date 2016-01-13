PLUGIN_TYPE = platforms
load(qt_plugin)

QT += waylandclient-private

OTHER_FILES += \
    qwayland-generic.json

SOURCES += main.cpp

