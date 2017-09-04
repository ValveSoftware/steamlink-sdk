android|ios|winrt {
    error( "This example is not supported for android, ios, or winrt." )
}

!include( ../tests.pri ) {
    error( "Couldn't find the tests.pri file!" )
}

SOURCES += main.cpp \
    galaxydata.cpp \
    star.cpp \
    cumulativedistributor.cpp

HEADERS += \
    cumulativedistributor.h \
    galaxydata.h \
    star.h

QT += widgets

OTHER_FILES += doc/src/* \
               doc/images/*

