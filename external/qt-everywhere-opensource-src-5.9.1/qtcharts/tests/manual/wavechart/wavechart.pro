!include( ../../tests.pri ) {
    error( "Couldn't find the test.pri file!" )
}

TARGET = wavechart
SOURCES += main.cpp wavechart.cpp
HEADERS += wavechart.h
