QT = core testlib
TARGET = tst_qserialport
#CONFIG += testcase

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += serialport
} else {
    include($$QTSERIALPORT_PROJECT_ROOT/src/serialport/qt4support/serialport.prf)
}

SOURCES = tst_qserialport.cpp
