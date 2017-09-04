!include( ../tests.pri ) {
    error( "Couldn't find the tests.pri file!" )
}

# The .cpp file which was generated for your project. Feel free to hack it.
SOURCES += main.cpp

RESOURCES += qmlmultitest.qrc

OTHER_FILES += doc/src/* \
               doc/images/* \
               qml/qmlmultitest/*
