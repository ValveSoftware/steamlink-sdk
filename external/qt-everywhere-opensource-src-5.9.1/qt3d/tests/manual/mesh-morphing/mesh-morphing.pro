!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dquick 3dinput quick qml 3dextras 3dquickextras 3danimation 3dquickanimation

SOURCES += \
    main.cpp

RESOURCES +=
