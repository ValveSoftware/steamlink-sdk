!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += qml quick
CONFIG += resources_big

SOURCES += \
    main.cpp

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
    android/AndroidManifest.xml

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
