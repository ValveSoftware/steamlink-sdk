TARGET = gltfsceneio
QT += core-private 3dcore 3dcore-private 3drender 3drender-private 3dextras

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    gltfio.h

SOURCES += \
    main.cpp \
    gltfio.cpp

DISTFILES += \
    gltf.json

PLUGIN_TYPE = sceneparsers
PLUGIN_CLASS_NAME = GLTFSceneIOPlugin
load(qt_plugin)
