!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCE += main.cpp

QT += qml quick 3dcore 3drender 3dinput 3dquick 3dquickextras

OTHER_FILES += main.qml \
               PlaneEntity.qml

SOURCES += \
    main.cpp

RESOURCES += \
    lights.qrc \
    ../exampleresources/obj.qrc \
    ../exampleresources/textures.qrc
