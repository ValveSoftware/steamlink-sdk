TARGET     = QtX11Extras

DEFINES += QT_NO_USING_NAMESPACE

QMAKE_DOCS = $$PWD/doc/qtx11extras.qdocconf

QT_PRIVATE += gui-private

HEADERS += qx11info_x11.h
SOURCES += qx11info_x11.cpp

load(qt_module)
