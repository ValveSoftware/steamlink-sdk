!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dextras 3dquickextras

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml \
    MyEntity.qml

RESOURCES += \
    bigmodel-qml.qrc

DISTFILES += \
    MyEntity.qml
