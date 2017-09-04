!include( ../auto.pri ) {
    error( "Couldn't find the auto.pri file!" )
}
SOURCES += tst_qml.cpp
QT += qml quick
contains(QT_COORD_TYPE, float): DEFINES += QT_QREAL_IS_FLOAT
