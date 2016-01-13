INCLUDEPATH += $$PWD

contains(QT_CONFIG, no-pkg-config) {
    LIBS += -lEGL -lwayland-client
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += egl wayland-client
}

SOURCES += \
        $$PWD/drmeglserverbufferintegration.cpp

HEADERS += \
        $$PWD/drmeglserverbufferintegration.h

CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += $$PWD/../../../extensions/drm-egl-server-buffer.xml
