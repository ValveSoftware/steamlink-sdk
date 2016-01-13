TARGET = wayland_cursor
QT = core

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-cursor
} else {
    LIBS += -lwayland-cursor
}

# Input
SOURCES += main.cpp
