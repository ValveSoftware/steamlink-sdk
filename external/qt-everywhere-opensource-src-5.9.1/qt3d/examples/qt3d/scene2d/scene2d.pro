!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += qml quick 3dquick 3dquickextras

SOURCES += main.cpp

RESOURCES += scene2d.qrc

OTHER_FILES += \
    main.qml \
    Logo.qml \
    LogoControls.qml \
    doc/src/*
