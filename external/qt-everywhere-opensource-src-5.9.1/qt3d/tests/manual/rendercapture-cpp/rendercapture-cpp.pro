!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dextras widgets

SOURCES += \
    main.cpp \
    orbittransformcontroller.cpp

HEADERS += \
    orbittransformcontroller.h \
    mycapture.h
