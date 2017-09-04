CONFIG += console
QT += core-private gui-private

qtHaveModule(widgets): QT += widgets

qtHaveModule(network) {
    QT += network
    DEFINES += NETWORK_DIAG
}

SOURCES += main.cpp qtdiag.cpp
HEADERS += qtdiag.h

load(qt_app)
