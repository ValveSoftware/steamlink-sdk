TARGET = gltfgeometryloader
QT += core-private 3dcore 3dcore-private 3drender 3drender-private

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    gltfgeometryloader.h \

SOURCES += \
    main.cpp \
    gltfgeometryloader.cpp \

DISTFILES += \
    gltf.json

PLUGIN_TYPE = geometryloaders
PLUGIN_CLASS_NAME = GLTFGeometryLoaderPlugin
load(qt_plugin)
