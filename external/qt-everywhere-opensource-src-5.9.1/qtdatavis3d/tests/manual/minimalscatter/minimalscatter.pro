!include( ../tests.pri ) {
    error( "Couldn't find the tests.pri file!" )
}

QT += core gui datavisualization

TARGET = MinimalScatter
TEMPLATE = app

SOURCES += ../../../src/datavisualization/doc/snippets/doc_src_q3dscatter_construction.cpp
