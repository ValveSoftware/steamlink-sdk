TARGET = xkbcommon
QT = core

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += xkbcommon
} else {
    LIBS += -lxkbcommon
}

# Input
SOURCES += main.cpp
