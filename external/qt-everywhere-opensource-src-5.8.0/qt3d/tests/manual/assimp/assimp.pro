!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

SOURCE += main.cpp

QT += qml quick 3dcore 3drender 3dinput 3dquick 3dextras 3dquickextras

OTHER_FILES += main.qml

SOURCES += \
    main.cpp

RESOURCES += \
    assimp.qrc \
    ../../../examples/qt3d/exampleresources/test_scene.qrc \
    ../../../examples/qt3d/exampleresources/chest.qrc
