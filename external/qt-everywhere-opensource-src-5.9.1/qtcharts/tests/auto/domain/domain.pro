!include( ../auto.pri ) {
    error( "Couldn't find the auto.pri file!" )
}

QT += charts-private

SOURCES += tst_domain.cpp
