!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

TEMPLATE = app

QT += qml quick 3dcore 3drender 3dinput 3dquick 3dextras 3dquickextras

CONFIG += c++11

SOURCES += main.cpp

RESOURCES += qml.qrc
