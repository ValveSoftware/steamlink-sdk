!include( ../tests.pri ) {
    error( "Couldn't find the tests.pri file!" )
}

SOURCES += main.cpp scatterchart.cpp
HEADERS += scatterchart.h

QT += widgets
