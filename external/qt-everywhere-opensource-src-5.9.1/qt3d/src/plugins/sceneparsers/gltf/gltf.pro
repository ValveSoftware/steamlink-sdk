TARGET = gltfsceneimport
QT += core-private 3dcore 3dcore-private 3drender 3drender-private 3dextras

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    gltfimporter.h

SOURCES += \
    main.cpp \
    gltfimporter.cpp

DISTFILES += \
    gltf.json

PLUGIN_TYPE = sceneparsers
PLUGIN_CLASS_NAME = GLTFSceneImportPlugin
load(qt_plugin)
