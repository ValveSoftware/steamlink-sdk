!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

RESOURCES += \
    skybox.qrc \
    ../../../examples/qt3d/exampleresources/cubemaps.qrc

SOURCES += \
    main.cpp
