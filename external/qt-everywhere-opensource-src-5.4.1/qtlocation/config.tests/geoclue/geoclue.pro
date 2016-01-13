TEMPLATE = app
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += geoclue
}
SOURCES += main.cpp
