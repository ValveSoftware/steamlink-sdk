!include( ../../../examples/qt3d/examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick

SOURCES += \
    main.cpp

OTHER_FILES += \
    *.qml

RESOURCES += \
    qt3dbench-qml.qrc
