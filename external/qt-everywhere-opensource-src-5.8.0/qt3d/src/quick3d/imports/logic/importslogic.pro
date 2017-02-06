CXX_MODULE = qml
TARGET = quick3dlogicplugin
TARGETPATH = Qt3D/Logic
IMPORT_VERSION = 2.0

QT += core-private qml 3dcore 3dlogic

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

OTHER_FILES += qmldir

HEADERS += \
    qt3dquick3dlogicplugin.h

SOURCES += \
    qt3dquick3dlogicplugin.cpp

load(qml_plugin)
