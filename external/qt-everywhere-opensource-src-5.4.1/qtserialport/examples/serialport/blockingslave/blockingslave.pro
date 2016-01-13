greaterThan(QT_MAJOR_VERSION, 4) {
    QT       += widgets serialport
} else {
    include($$QTSERIALPORT_PROJECT_ROOT/src/serialport/qt4support/serialport.prf)
}

TARGET = blockingslave
TEMPLATE = app

HEADERS += \
    dialog.h \
    slavethread.h

SOURCES += \
    main.cpp \
    dialog.cpp \
    slavethread.cpp
