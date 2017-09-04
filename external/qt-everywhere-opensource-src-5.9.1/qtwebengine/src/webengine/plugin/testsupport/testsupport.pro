CXX_MODULE = qml
TARGET = qtwebenginetestsupportplugin
TARGETPATH = QtWebEngine/testsupport
IMPORT_VERSION = 1.0

QT += webengine qml quick
QT_PRIVATE += webengine-private

INCLUDEPATH += $$QTWEBENGINE_ROOT/src/core $$QTWEBENGINE_ROOT/src/webengine $$QTWEBENGINE_ROOT/src/webengine/api

SOURCES = plugin.cpp

load(qml_plugin)
