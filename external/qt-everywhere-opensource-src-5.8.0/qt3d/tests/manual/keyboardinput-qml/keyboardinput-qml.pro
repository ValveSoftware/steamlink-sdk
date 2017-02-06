!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dquick 3dinput quick qml 3dquickextras

SOURCES += \
    main.cpp

RESOURCES += \
    keyboardinput-qml.qrc

OTHER_FILES += \
    main.qml \
    SphereEntity.qml
