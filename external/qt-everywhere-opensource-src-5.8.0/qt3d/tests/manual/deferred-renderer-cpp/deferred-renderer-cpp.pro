!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dextras

HEADERS += \
    gbuffer.h \
    deferredrenderer.h \
    finaleffect.h \
    sceneeffect.h \
    pointlightblock.h \
    screenquadentity.h \
    sceneentity.h

SOURCES += \
    main.cpp \
    gbuffer.cpp \
    deferredrenderer.cpp \
    finaleffect.cpp \
    sceneeffect.cpp \
    pointlightblock.cpp \
    screenquadentity.cpp \
    sceneentity.cpp

RESOURCES += \
    deferred-renderer-cpp.qrc

OTHER_FILES += \
    geometry_gl2.vert \
    geometry_gl2.frag \
    geometry_gl3.frag \
    geometry_gl3.vert \
    final_gl2.vert \
    final_gl2.frag \
    final_gl3.frag \
    final_gl3.vert
