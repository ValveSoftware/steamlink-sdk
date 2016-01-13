include($$PWD/../xcomposite_share/xcomposite_share.pri)

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += xcomposite gl x11 wayland-server
} else {
    LIBS += -lXcomposite -lGL -lX11
}

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/xcompositeglxintegration.h

SOURCES += \
    $$PWD/xcompositeglxintegration.cpp
