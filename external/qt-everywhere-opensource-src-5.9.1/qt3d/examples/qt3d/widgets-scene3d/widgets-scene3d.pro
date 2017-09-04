!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += widgets qml quick quickwidgets 3dinput

SOURCES += \
    main.cpp

OTHER_FILES += \
    ../scene3d/AnimatedEntity.qml \
    ../scene3d/main.qml

RESOURCES += \
    widgets-scene3d.qrc
