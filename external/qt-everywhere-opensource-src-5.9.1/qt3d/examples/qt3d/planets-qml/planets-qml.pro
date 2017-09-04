!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += qml quick \
    concurrent \
    3dcore 3drender 3dinput \
    3dquick 3dquickrender 3dquickinput 3dquickextras \
    network

CONFIG += resources_big c++11

HEADERS += \
    networkcontroller.h

SOURCES += \
    main.cpp \
    networkcontroller.cpp

OTHER_FILES += \
    *.qml \
    planets.js \
    shaders/gl3/*.* \
    shaders/es2/*.* \
    doc/src/*.* \
    doc/images/*.*

RESOURCES += \
    planets-qml.qrc \
    planets-qml-images.qrc

DISTFILES += \
    PlanetsMain.qml \
    android/AndroidManifest.xml \
    images/license.txt

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
