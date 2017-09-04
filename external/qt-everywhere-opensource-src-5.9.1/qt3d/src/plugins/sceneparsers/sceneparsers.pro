TEMPLATE = subdirs
# QNX is not supported, and Linux GCC 4.9 on ARM chokes on the assimp
# sources (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66964).
QT_FOR_CONFIG += 3dcore-private
qtConfig(assimp):if(qtConfig(system-assimp)|!cross_compile): \
    SUBDIRS += assimp
SUBDIRS += gltf

qtConfig(temporaryfile):qtConfig(regularexpression) {
    SUBDIRS += gltfexport
}
