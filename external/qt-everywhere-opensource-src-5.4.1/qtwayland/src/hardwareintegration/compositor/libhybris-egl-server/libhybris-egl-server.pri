INCLUDEPATH += $$PWD

contains(QT_CONFIG, no-pkg-config) {
    LIBS += -lwayland-server -lEGL
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-server egl
}

SOURCES += \
    $$PWD/libhybriseglserverbufferintegration.cpp


HEADERS += \
    $$PWD/libhybriseglserverbufferintegration.h

CONFIG += wayland-scanner
WAYLANDSERVERSOURCES += $$PWD/../../../extensions/libhybris-egl-server-buffer.xml
