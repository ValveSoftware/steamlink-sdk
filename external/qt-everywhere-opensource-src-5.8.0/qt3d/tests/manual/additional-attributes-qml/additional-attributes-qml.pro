!include( ../manual.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick 3dlogic qml quick 3dquickextras

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml

RESOURCES += \
    additional-attributes-qml.qrc
