!include( ../auto.pri ) {
    error( "Couldn't find the auto.pri file!" )
}

QT += charts-private
contains(QT_COORD_TYPE, float): DEFINES += QT_QREAL_IS_FLOAT

SOURCES += tst_chartdataset.cpp
