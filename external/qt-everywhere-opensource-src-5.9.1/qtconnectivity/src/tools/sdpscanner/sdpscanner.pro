TEMPLATE = app
TARGET = sdpscanner

QT = core

SOURCES = main.cpp

QT_FOR_CONFIG += bluetooth-private
QMAKE_USE += bluez

load(qt_tool)

linux-*: {
    # bluetooth.h is not standards compliant
    CONFIG -= strict_c++
}
