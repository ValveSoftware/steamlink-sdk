CXX_MODULE = qml
TARGET = qtqmlremoteobjects
TARGETPATH = QtQml/RemoteObjects
IMPORT_VERSION = 1.0

QT += qml remoteobjects

SOURCES = \
    $$PWD/plugin.cpp \

HEADERS = \

load(qml_plugin)
