!include( ../manual.pri ) {
    error( "Couldn't find the manual.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

HEADERS += \


OTHER_FILES += \
    main.qml \
    DeferredRenderer.qml \
    FinalEffect.qml \
    SceneEffect.qml \
    GBuffer.qml

RESOURCES += \
    deferred-renderer-qml.qrc

SOURCES += \
    main.cpp
