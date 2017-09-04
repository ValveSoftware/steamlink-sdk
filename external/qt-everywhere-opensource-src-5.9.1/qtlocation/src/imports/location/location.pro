QT += quick-private network positioning-private location-private qml-private core-private gui-private

SOURCES += \
           location.cpp

load(qml_plugin)

OTHER_FILES += \
    plugin.json \
    qmldir
