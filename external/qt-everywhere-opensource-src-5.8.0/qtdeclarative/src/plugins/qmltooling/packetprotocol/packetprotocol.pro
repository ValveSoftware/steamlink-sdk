TARGET = QtPacketProtocol
QT     = core-private qml-private
CONFIG += static internal_module

HEADERS = \
    qpacketprotocol_p.h \
    qpacket_p.h

SOURCES = \
    qpacketprotocol.cpp \
    qpacket.cpp

load(qt_module)
