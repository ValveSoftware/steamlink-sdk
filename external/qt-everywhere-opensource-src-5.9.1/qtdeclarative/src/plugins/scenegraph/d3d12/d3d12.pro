TARGET = qsgd3d12backend

QT += core-private gui-private qml-private quick-private

PLUGIN_TYPE = scenegraph
PLUGIN_CLASS_NAME = QSGD3D12Adaptation
load(qt_plugin)

QMAKE_TARGET_PRODUCT = "Qt Quick D3D12 Renderer (Qt $$QT_VERSION)"
QMAKE_TARGET_DESCRIPTION = "Quick D3D12 Renderer for Qt."

SOURCES += \
    $$PWD/qsgd3d12adaptation.cpp \
    $$PWD/qsgd3d12renderloop.cpp \
    $$PWD/qsgd3d12threadedrenderloop.cpp \
    $$PWD/qsgd3d12renderer.cpp \
    $$PWD/qsgd3d12context.cpp \
    $$PWD/qsgd3d12rendercontext.cpp \
    $$PWD/qsgd3d12internalrectanglenode.cpp \
    $$PWD/qsgd3d12material.cpp \
    $$PWD/qsgd3d12builtinmaterials.cpp \
    $$PWD/qsgd3d12texture.cpp \
    $$PWD/qsgd3d12internalimagenode.cpp \
    $$PWD/qsgd3d12glyphnode.cpp \
    $$PWD/qsgd3d12glyphcache.cpp \
    $$PWD/qsgd3d12layer.cpp \
    $$PWD/qsgd3d12shadereffectnode.cpp \
    $$PWD/qsgd3d12painternode.cpp \
    $$PWD/qsgd3d12publicnodes.cpp \
    $$PWD/qsgd3d12spritenode.cpp

NO_PCH_SOURCES += \
    $$PWD/qsgd3d12engine.cpp

HEADERS += \
    $$PWD/qsgd3d12adaptation_p.h \
    $$PWD/qsgd3d12renderloop_p.h \
    $$PWD/qsgd3d12threadedrenderloop_p.h \
    $$PWD/qsgd3d12renderer_p.h \
    $$PWD/qsgd3d12context_p.h \
    $$PWD/qsgd3d12rendercontext_p.h \
    $$PWD/qsgd3d12engine_p.h \
    $$PWD/qsgd3d12engine_p_p.h \
    $$PWD/qsgd3d12internalrectanglenode_p.h \
    $$PWD/qsgd3d12material_p.h \
    $$PWD/qsgd3d12builtinmaterials_p.h \
    $$PWD/qsgd3d12texture_p.h \
    $$PWD/qsgd3d12internalimagenode_p.h \
    $$PWD/qsgd3d12glyphnode_p.h \
    $$PWD/qsgd3d12glyphcache_p.h \
    $$PWD/qsgd3d12layer_p.h \
    $$PWD/qsgd3d12shadereffectnode_p.h \
    $$PWD/qsgd3d12painternode_p.h \
    $$PWD/qsgd3d12publicnodes_p.h \
    $$PWD/qsgd3d12spritenode_p.h

LIBS += -ldxgi -ld3d12 -ld3dcompiler -ldcomp

include($$PWD/shaders/shaders.pri)

OTHER_FILES += $$PWD/d3d12.json
