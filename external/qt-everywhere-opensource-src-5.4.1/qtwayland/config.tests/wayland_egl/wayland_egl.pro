TARGET = wayland_egl
QT = core

!contains(QT_CONFIG, opengl): error("wayland_egl support requires Qt configured with OpenGL")
!contains(QT_CONFIG, egl): error("wayland_egl support requires Qt configured with EGL")

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-egl
} else {
    LIBS += -lwayland-egl
}

# Input
SOURCES += main.cpp
