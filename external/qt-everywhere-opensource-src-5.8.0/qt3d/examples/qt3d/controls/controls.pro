!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += qml quick

SOURCES += main.cpp

RESOURCES += controls.qrc

OTHER_FILES += main.qml \
               Logo.qml
