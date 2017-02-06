!include( ../tests.pri ) {
    error( "Couldn't find the tests.pri file!" )
}

QT += core gui datavisualization

TARGET = minimalSurface
TEMPLATE = app

SOURCES += ../../../src/datavisualization/doc/snippets/doc_src_q3dsurface_construction.cpp
