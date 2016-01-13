TARGET = wayland
QT = core

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-client
} else {
    LIBS += -lwayland-client
}

# Input
SOURCES += main.cpp