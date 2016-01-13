INCLUDEPATH += $$PWD
include($$PWD/../xcomposite_share/xcomposite_share.pri)

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-client xcomposite egl x11
} else {
    LIBS += -lXcomposite -lEGL -lX11
}

SOURCES += \
    $$PWD/qwaylandxcompositeeglcontext.cpp \
    $$PWD/qwaylandxcompositeeglclientbufferintegration.cpp \
    $$PWD/qwaylandxcompositeeglwindow.cpp

HEADERS += \
    $$PWD/qwaylandxcompositeeglcontext.h \
    $$PWD/qwaylandxcompositeeglclientbufferintegration.h \
    $$PWD/qwaylandxcompositeeglwindow.h
