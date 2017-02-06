TEMPLATE = subdirs
# QNX is not supported, and Linux GCC 4.9 on ARM chokes on the assimp
# sources (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66964).
config_assimp|!cross_compile: SUBDIRS += assimp
SUBDIRS += gltf
