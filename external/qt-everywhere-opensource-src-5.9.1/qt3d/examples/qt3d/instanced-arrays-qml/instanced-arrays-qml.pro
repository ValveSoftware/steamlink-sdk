!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += qml quick 3dcore 3drender 3dinput 3dquick 3dquickextras

SOURCES += \
    main.cpp \
    instancebuffer.cpp

RESOURCES += \
    instanced-arrays-qml.qrc

OTHER_FILES += *.qml

HEADERS += \
    instancebuffer.h
