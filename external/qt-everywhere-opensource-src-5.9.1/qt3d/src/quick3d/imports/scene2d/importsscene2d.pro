CXX_MODULE = qml
TARGET = qtquickscene2dplugin
TARGETPATH = QtQuick/Scene2D
IMPORT_VERSION = 2.0

QT += qml quick 3dcore 3drender 3drender-private 3dinput 3dlogic 3dquickscene2d 3dquickscene2d-private

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    qtquickscene2dplugin.h \

SOURCES += \
    qtquickscene2dplugin.cpp \

OTHER_FILES += qmldir

load(qml_plugin)
