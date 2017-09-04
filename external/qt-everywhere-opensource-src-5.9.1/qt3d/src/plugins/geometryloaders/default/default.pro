TARGET = defaultgeometryloader
QT += core-private 3dcore 3dcore-private 3drender 3drender-private

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    basegeometryloader_p.h \

SOURCES += \
    main.cpp \
    basegeometryloader.cpp \
    objgeometryloader.cpp \
    plygeometryloader.cpp \
    stlgeometryloader.cpp \

DISTFILES += \
    default.json

PLUGIN_TYPE = geometryloaders
PLUGIN_CLASS_NAME = DefaultGeometryLoaderPlugin
load(qt_plugin)
