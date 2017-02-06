!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

# The .cpp file which was generated for your project. Feel free to hack it.
SOURCES += main.cpp

RESOURCES += qmlsurface.qrc

OTHER_FILES += doc/src/* \
               doc/images/* \
               qml/qmlsurface/*
