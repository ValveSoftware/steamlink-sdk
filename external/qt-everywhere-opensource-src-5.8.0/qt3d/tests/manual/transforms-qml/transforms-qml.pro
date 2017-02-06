!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml \
    RenderableEntity.qml \
    TemporaryCamera.qml \
    TrefoilKnot.qml

RESOURCES += \
    transforms-qml.qrc \
    ../../../examples/qt3d/exampleresources/obj.qrc
