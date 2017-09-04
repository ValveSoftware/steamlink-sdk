!include( ../../tests.pri ) {
    error( "Couldn't find the test.pri file!" )
}

TARGET = presenterchart
HEADERS += chartview.h
SOURCES += main.cpp chartview.cpp

QT += widgets
