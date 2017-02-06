!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dquick 3dinput qml quick 3dquickextras

HEADERS += \

SOURCES += \
    main.cpp

OTHER_FILES += \
    shaders/* \
    main.qml \
    BasicCamera.qml \
    WaveForwardRenderer.qml \
    Wave.qml \
    WaveEffect.qml \
    WaveMaterial.qml \
    Background.qml \
    BackgroundEffect.qml \
    shaders/background.vert \
    shaders/background.frag

RESOURCES += \
    wave.qrc
