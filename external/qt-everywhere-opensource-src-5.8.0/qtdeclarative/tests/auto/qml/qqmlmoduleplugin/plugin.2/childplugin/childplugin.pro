TEMPLATE = lib
CONFIG += childplugin
SOURCES = childplugin.cpp
QT = core qml
DESTDIR = ../../imports/org/qtproject/AutoTestQmlPluginType.2/ChildPlugin

QT += core-private gui-private qml-private

IMPORT_DIR = DESTDIR
IMPORT_FILES = \
        qmldir

include (../../../../shared/imports.pri)
