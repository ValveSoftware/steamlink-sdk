CXX_MODULE = qml
TARGET  = qmllocalstorageplugin
TARGETPATH = QtQuick/LocalStorage
IMPORT_VERSION = 2.0

QT = sql qml-private  core-private

SOURCES += plugin.cpp

load(qml_plugin)

OTHER_FILES += localstorage.json
