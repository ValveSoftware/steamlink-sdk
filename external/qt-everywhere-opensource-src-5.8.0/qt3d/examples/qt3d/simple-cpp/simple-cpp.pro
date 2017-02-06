!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dextras

SOURCES += \
    main.cpp \
    orbittransformcontroller.cpp

HEADERS += \
    orbittransformcontroller.h
