TEMPLATE = app
TARGET = sdpscanner

QT = core

SOURCES = main.cpp

CONFIG += link_pkgconfig
PKGCONFIG_PRIVATE += bluez

load(qt_tool)

linux-*: {
    # bluetooth.h is not standards compliant
    contains(QMAKE_CXXFLAGS, -std=c++0x) {
        QMAKE_CXXFLAGS -= -std=c++0x
        QMAKE_CXXFLAGS += -std=gnu++0x
        CONFIG -= c++11
    }
    c++11 {
        CONFIG -= c++11
        QMAKE_CXXFLAGS += -std=gnu++0x
    }
}
