TEMPLATE = app
TARGET = client
INCLUDEPATH += .

QT += waylandclient-private
CONFIG += link_pkgconfig
PKGCONFIG += wayland-client

CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += ../share-buffer.xml

SOURCES += \
    main.cpp \
    serverbufferrenderer.cpp

HEADERS += \
    serverbufferrenderer.h
