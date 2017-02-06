!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCE += main.cpp

QT += qml quick 3dcore 3drender 3dinput 3dquick 3dextras 3dquickextras

OTHER_FILES += *.qml

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc \
    ../exampleresources/cubemaps.qrc \
    ../exampleresources/obj.qrc

DISTFILES += \
    StereoFrameGraph.qml
