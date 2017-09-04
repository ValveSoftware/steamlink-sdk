TEMPLATE = subdirs
QT_FOR_CONFIG += 3dcore-private
!android:qtConfig(assimp):qtConfig(commandlineparser): \
    SUBDIRS += qgltf
