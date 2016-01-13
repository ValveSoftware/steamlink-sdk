TARGET = libhybris_egl_server
QT = core

!contains(QT_CONFIG, opengl): error("libhybris_egl_server support requires Qt configured with OpenGL")

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += egl
} else {
    LIBS += -legl
}

# Input
SOURCES += main.cpp
