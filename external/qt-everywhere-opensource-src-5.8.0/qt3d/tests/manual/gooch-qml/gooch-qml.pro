!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dquick 3dinput qml quick 3dquickextras

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml \
    MyEntity.qml

RESOURCES += \
    gooch-qml.qrc \
    ../../../examples/qt3d/exampleresources/obj.qrc
