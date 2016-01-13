TEMPLATE = app
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += gypsy gconf-2.0
}
SOURCES += main.cpp
