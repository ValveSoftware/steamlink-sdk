requires(qtHaveModule(multimedia))

!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3dquick qml quick multimedia

SOURCES += \
    main.cpp \
    touchsettings.cpp

HEADERS += \
    touchsettings.h

OTHER_FILES += \
    *.qml

RESOURCES += \
    audio-visualizer-qml.qrc
