!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick qml quick 3dquickextras

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml \
    AdsMaterial.qml \
    AdsEffect.qml \
    ShadowMapLight.qml \
    ShadowMapFrameGraph.qml \
    Trefoil.qml \
    Toyplane.qml \
    GroundPlane.qml

RESOURCES += \
    shadow-map-qml.qrc \
    ../exampleresources/obj.qrc
