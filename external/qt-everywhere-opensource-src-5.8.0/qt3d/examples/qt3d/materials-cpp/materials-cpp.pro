!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dextras

HEADERS += \
    planeentity.h \
    renderableentity.h \
    trefoilknot.h \
    barrel.h \
    rotatingtrefoilknot.h \
    houseplant.h

SOURCES += \
    main.cpp \
    planeentity.cpp \
    renderableentity.cpp \
    trefoilknot.cpp \
    barrel.cpp \
    rotatingtrefoilknot.cpp \
    houseplant.cpp

RESOURCES += \
    ../exampleresources/chest.qrc \
    ../exampleresources/houseplants.qrc \
    ../exampleresources/metalbarrel.qrc \
    ../exampleresources/obj.qrc \
    ../exampleresources/textures.qrc
