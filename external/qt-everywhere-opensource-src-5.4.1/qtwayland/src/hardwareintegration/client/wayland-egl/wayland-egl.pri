INCLUDEPATH += $$PWD
!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-client wayland-egl egl
} else {
    LIBS += -lwayland-egl -lEGL
}

SOURCES += $$PWD/qwaylandeglclientbufferintegration.cpp \
           $$PWD/qwaylandglcontext.cpp \
           $$PWD/qwaylandeglwindow.cpp

HEADERS += $$PWD/qwaylandeglclientbufferintegration.h \
           $$PWD/qwaylandglcontext.h \
           $$PWD/qwaylandeglwindow.h \
           $$PWD/qwaylandeglinclude.h
