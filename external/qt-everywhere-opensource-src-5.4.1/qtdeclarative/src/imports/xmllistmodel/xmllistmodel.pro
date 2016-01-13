CXX_MODULE = qml
TARGET  = qmlxmllistmodelplugin
TARGETPATH = QtQuick/XmlListModel
IMPORT_VERSION = 2.0

QT = network xmlpatterns qml-private  core-private

SOURCES += qqmlxmllistmodel.cpp plugin.cpp
HEADERS += qqmlxmllistmodel_p.h

load(qml_plugin)
