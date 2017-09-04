!include( ../auto.pri ) {
    error( "Couldn't find the auto.pri file!" )
}
SOURCES += tst_qlineseries.cpp

!system_build:mac: QMAKE_POST_LINK += "$$MAC_POST_LINK_PREFIX $$MAC_AUTOTESTS_BIN_DIR"
