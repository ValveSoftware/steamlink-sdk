CXX_MODULE = qml
TARGET = quick3dinputplugin
TARGETPATH = Qt3D/Input
IMPORT_VERSION = 2.0

QT += core-private qml 3dcore 3dinput 3dinput-private 3dquickinput-private

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

OTHER_FILES += qmldir

HEADERS += \
    qt3dquick3dinputplugin.h

SOURCES += \
    qt3dquick3dinputplugin.cpp

qtHaveModule(gamepad): DEFINES += HAVE_QGAMEPAD

load(qml_plugin)
