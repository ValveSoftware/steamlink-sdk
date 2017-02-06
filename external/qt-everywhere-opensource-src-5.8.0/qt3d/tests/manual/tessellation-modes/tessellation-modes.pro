!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

HEADERS += \
    tessellatedquadmesh.h

SOURCES += \
    main.cpp \
    tessellatedquadmesh.cpp

OTHER_FILES += \
    main.qml \
    BasicCamera.qml \
    TessellatedWireframeEffect.qml \
    TessellatedWireframeMaterial.qml \
    TessellatedQuad.qml \
    shaders/*

RESOURCES += \
    tessellation-modes.qrc
