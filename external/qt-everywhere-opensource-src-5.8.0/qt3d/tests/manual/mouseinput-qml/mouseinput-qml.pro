!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

SOURCES += main.cpp

RESOURCES += \
    mouseinput-qml.qrc

OTHER_FILES += \
    main.qml
