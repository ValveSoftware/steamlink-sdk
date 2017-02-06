TEMPLATE = subdirs

SUBDIRS += \
    assimp \
    bigscene-cpp \
    bigmodel-qml \
    bigscene-instanced-qml \
    clip-planes-qml \
    component-changes \
    custom-mesh-cpp \
    custom-mesh-qml \
    custom-mesh-update-data-cpp \
    custom-mesh-update-data-qml \
    cylinder-cpp \
    cylinder-qml \
    deferred-renderer-cpp \
    deferred-renderer-qml \
    dynamicscene-cpp \
    enabled-qml \
    gltf \
    gooch-qml \
    keyboardinput-qml \
    loader-qml \
    mouseinput-qml \
    multiplewindows-qml \
    picking-qml \
    plasma \
    scene3d-loader \
    simple-shaders-qml \
    skybox \
    tessellation-modes \
    transforms-qml \
    transparency-qml \
    transparency-qml-scene3d \
    rendercapture-qml \
    additional-attributes-qml \
    dynamic-model-loader-qml

qtHaveModule(widgets): {
    SUBDIRS += \
        assimp-cpp \
        paintedtexture-cpp \
        rendercapture-cpp
}
