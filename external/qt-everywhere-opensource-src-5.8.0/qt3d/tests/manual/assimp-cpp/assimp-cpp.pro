android|ios|tvos|winrt {
    warning( "This example is not supported for android, ios, tvos, or winrt." )
}

!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += widgets 3dcore 3drender 3dinput 3dextras

SOURCES += main.cpp
