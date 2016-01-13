QT += sensors
TARGET = qtsensorgestures_counterplugin

QTDIR_build {
# This is only for the Qt build. Do not use externally. We mean it.
PLUGIN_TYPE = sensorgestures
PLUGIN_CLASS_NAME = QCounterGesturePlugin
PLUGIN_EXTENDS = -
load(qt_plugin)
} else {

TEMPLATE = lib
CONFIG += plugin

target.path += $$[QT_INSTALL_PLUGINS]/sensorgestures
INSTALLS += target

}

HEADERS += \
    qcountergestureplugin.h \
    qcounterrecognizer.h
SOURCES += \
    qcountergestureplugin.cpp \
    qcounterrecognizer.cpp

OTHER_FILES += \
    plugin.json

