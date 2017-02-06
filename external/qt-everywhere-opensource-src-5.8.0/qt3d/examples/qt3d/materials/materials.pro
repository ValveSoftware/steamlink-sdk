!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

HEADERS += \

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml \
    *.qml \
    HousePlant.qml

RESOURCES += \
    materials.qrc \
    ../exampleresources/chest.qrc \
    ../exampleresources/houseplants.qrc \
    ../exampleresources/metalbarrel.qrc \
    ../exampleresources/obj.qrc \
    ../exampleresources/textures.qrc
