android|ios|winrt {
    error( "This example is not supported for android, ios, or winrt." )
}

!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCES += main.cpp \
           surfacegraph.cpp

HEADERS += surfacegraph.h

QT += widgets

RESOURCES += surface.qrc

OTHER_FILES += doc/src/* \
               doc/images/*
