CXX_MODULE = qml
TARGET = quick3dcoreplugin
TARGETPATH = Qt3D/Core
IMPORT_VERSION = 2.0

QT += core-private qml qml-private quick quick-private 3dcore 3dcore-private 3dquick 3dquick-private

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    qt3dquick3dcoreplugin.h

SOURCES += \
    qt3dquick3dcoreplugin.cpp

OTHER_FILES += qmldir

load(qml_plugin)
