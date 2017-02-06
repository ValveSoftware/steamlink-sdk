!include( ../auto.pri ) {
    error( "Couldn't find the auto.pri file!" )
}
HEADERS += ../qxyseries/tst_qxyseries.h
SOURCES += tst_qlineseries.cpp ../qxyseries/tst_qxyseries.cpp
