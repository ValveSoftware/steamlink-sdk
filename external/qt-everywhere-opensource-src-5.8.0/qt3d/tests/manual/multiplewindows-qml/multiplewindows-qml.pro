!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

TEMPLATE = app

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

CONFIG += c++11

SOURCES += main.cpp

RESOURCES += qml.qrc

DISTFILES += \
    Scene.qml
