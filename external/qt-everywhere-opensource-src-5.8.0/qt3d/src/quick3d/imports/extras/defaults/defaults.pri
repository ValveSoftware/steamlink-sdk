
# When adding new QML files that should be built into the plugin,
# add them to this variable and they will be listed into a generated
# resource file.
#
# To have the plugin register them as types, add an entries to the
# qmldir array in qt3dquick3dextrasplugin.cpp
QML_FILES = \
    $$PWD/qml/DefaultEffect.qml \
    $$PWD/qml/DefaultAlphaEffect.qml \
    $$PWD/qml/PhongMaterial.qml \
    $$PWD/qml/DiffuseMapMaterial.qml \
    $$PWD/qml/DiffuseSpecularMapMaterial.qml \
    $$PWD/qml/NormalDiffuseMapMaterial.qml \
    $$PWD/qml/NormalDiffuseMapAlphaMaterial.qml \
    $$PWD/qml/NormalDiffuseSpecularMapMaterial.qml \
    $$PWD/qml/ForwardRenderer.qml \
    $$PWD/qml/PerVertexColorMaterial.qml \
    $$PWD/qml/SkyboxEntity.qml \
    $$PWD/qml/GoochMaterial.qml \
    $$PWD/qml/PhongAlphaMaterial.qml \
    $$PWD/qml/TextureMaterial.qml \
    $$PWD/qml/OrbitCameraController.qml \
    $$PWD/qml/FirstPersonCameraController.qml \
    $$PWD/qml/NormalDiffuseMapAlphaEffect.qml
