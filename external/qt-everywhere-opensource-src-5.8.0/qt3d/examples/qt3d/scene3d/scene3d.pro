!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += qml quick 3dinput

SOURCES += \
    main.cpp

OTHER_FILES += \
    AnimatedEntity.qml \
    main.qml

RESOURCES += \
    scene3d.qrc
