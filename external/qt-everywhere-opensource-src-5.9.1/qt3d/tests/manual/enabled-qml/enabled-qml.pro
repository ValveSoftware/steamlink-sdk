!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dquick qml 3dquickextras

SOURCES += \
    main.cpp

RESOURCES += \
    enabled-qml.qrc

