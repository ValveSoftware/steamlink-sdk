!include( ../manual.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput

SOURCES += \
    main.cpp
