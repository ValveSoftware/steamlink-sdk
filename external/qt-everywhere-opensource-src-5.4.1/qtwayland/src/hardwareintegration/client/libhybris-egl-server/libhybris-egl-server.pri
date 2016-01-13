INCLUDEPATH += $$PWD

contains(QT_CONFIG, no-pkg-config) {
    LIBS += -lEGL -lwayland-client
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += egl wayland-client
}

SOURCES += \
        $$PWD/libhybriseglserverbufferintegration.cpp

HEADERS += \
        $$PWD/libhybriseglserverbufferintegration.h

CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += $$PWD/../../../extensions/libhybris-egl-server-buffer.xml
