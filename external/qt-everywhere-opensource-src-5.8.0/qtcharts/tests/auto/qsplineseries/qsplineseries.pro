!include( ../auto.pri ) {
    error( "Couldn't find the auto.pri file!" )
}
HEADERS += ../qxyseries/tst_qxyseries.h
SOURCES += tst_qsplineseries.cpp ../qxyseries/tst_qxyseries.cpp
