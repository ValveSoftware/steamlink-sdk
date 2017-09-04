CXX_MODULE = qml
TARGET = quick3danimationplugin
TARGETPATH = Qt3D/Animation
IMPORT_VERSION = 2.2

QT += core-private qml qml-private 3dcore 3drender 3danimation 3dquick 3dquickrender 3dquickanimation 3dquick-private 3dquickanimation-private

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    qt3dquick3danimationplugin.h

SOURCES += \
    qt3dquick3danimationplugin.cpp

OTHER_FILES += qmldir

load(qml_plugin)
