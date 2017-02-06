!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput 3dquick 3dquickextras qml quick

HEADERS += \

SOURCES += \
    main.cpp

OTHER_FILES += \
    main.qml \
    Scene.qml

QT3D_MODELS = ../exampleresources/assets/test_scene.dae \
              ../exampleresources/assets/obj/toyplane.obj \
              ../exampleresources/assets/obj/trefoil.obj

QGLTF_PARAMS = -b -S
load(qgltf)

RESOURCES += \
    qgltf_example.qrc
