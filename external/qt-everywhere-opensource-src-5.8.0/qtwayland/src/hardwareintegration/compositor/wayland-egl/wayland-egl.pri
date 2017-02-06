INCLUDEPATH += $$PWD

QMAKE_USE_PRIVATE += wayland-server wayland-egl

QT += egl_support-private

SOURCES += \
    $$PWD/waylandeglclientbufferintegration.cpp

HEADERS += \
    $$PWD/waylandeglclientbufferintegration.h
