TARGET = QtSerialPort
QT = core-private

QMAKE_DOCS = $$PWD/doc/qtserialport.qdocconf

config_ntddmodm: DEFINES += QT_NO_REDEFINE_GUID_DEVINTERFACE_MODEM

include($$PWD/serialport-lib.pri)

load(qt_module)

PRECOMPILED_HEADER =
