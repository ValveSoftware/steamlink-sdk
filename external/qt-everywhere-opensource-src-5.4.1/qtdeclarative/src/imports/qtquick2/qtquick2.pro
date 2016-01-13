CXX_MODULE = qml
TARGET  = qtquick2plugin
TARGETPATH = QtQuick.2

SOURCES += \
    plugin.cpp

QT += quick-private qml-private

load(qml_plugin)
