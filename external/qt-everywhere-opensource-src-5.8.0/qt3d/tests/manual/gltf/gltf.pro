!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

HEADERS += \

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml \
    Wine.qml

RESOURCES += \
    gltf_example.qrc \
    ../../../examples/qt3d/exampleresources/gltf.qrc
