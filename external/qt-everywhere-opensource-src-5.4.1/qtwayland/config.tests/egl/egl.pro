TARGET = egl
QT = core

!contains(QT_CONFIG, opengl): error("egl support requires Qt configured with OpenGL")
!contains(QT_CONFIG, egl): error("egl support requires Qt configured with EGL")

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += egl
} else {
    LIBS += -lEGL
}

# Input
SOURCES += main.cpp
