TEMPLATE = app

!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += qml quick
CONFIG += c++11

SOURCES += main.cpp

RESOURCES += qml.qrc \
    models.qrc \
    textures.qrc \
    shaders.qrc

