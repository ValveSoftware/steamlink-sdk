TEMPLATE = app
TARGET = tst_qserialport

QT = core testlib
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += serialport
} else {
    include($$QTSERIALPORT_PROJECT_ROOT/src/serialport/qt4support/serialport.prf)
}

SOURCES += tst_qserialport.cpp
