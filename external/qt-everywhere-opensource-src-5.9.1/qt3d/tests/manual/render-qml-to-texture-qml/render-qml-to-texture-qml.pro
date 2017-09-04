!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

!include ( ../render-qml-to-texture/render-qml-to-texture.pri ) {
    error( "Couldn't find the render-qml-to-texture.pri file!")
}

QT += 3dextras 3dcore 3drender 3dinput 3dquick qml quick

SOURCES += main.cpp

RESOURCES += \
    render-qml-to-texture-qml.qrc

OTHER_FILES += \
    main.qml

HEADERS +=


