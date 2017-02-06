!include( ../tests.pri ) {
    error( "Couldn't find the tests.pri file!" )
}

SOURCES += main.cpp

RESOURCES += qmldynamicdata.qrc

OTHER_FILES += qml/qmldynamicdata/*
