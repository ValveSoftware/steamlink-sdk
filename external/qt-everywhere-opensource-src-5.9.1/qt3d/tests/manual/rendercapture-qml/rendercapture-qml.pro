!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dextras 3dquick 3dlogic qml quick 3dquickextras

SOURCES += \
    main.cpp

DISTFILES += \
    main.qml

RESOURCES += \
    qml.qrc

HEADERS += \
    rendercaptureprovider.h

