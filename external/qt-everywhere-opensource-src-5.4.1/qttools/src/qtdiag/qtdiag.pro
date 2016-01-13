load(qt_app)

CONFIG += console
QT += core-private gui-private

qtHaveModule(network) {
    QT += network
    DEFINES += NETWORK_DIAG
}

SOURCES += main.cpp qtdiag.cpp
HEADERS += qtdiag.h
