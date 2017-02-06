
add_library(Qt5::GLTFSceneIOPlugin MODULE IMPORTED)

_populate_3DRender_plugin_properties(GLTFSceneIOPlugin RELEASE "sceneparsers/libgltfsceneio.so")

list(APPEND Qt53DRender_PLUGINS Qt5::GLTFSceneIOPlugin)
