INCLUDEPATH += $$PWD
include($$PWD/../xcomposite_share/xcomposite_share.pri)

QMAKE_USE += egl wayland-client

QT += egl_support-private

SOURCES += \
    $$PWD/qwaylandxcompositeeglcontext.cpp \
    $$PWD/qwaylandxcompositeeglclientbufferintegration.cpp \
    $$PWD/qwaylandxcompositeeglwindow.cpp

HEADERS += \
    $$PWD/qwaylandxcompositeeglcontext.h \
    $$PWD/qwaylandxcompositeeglclientbufferintegration.h \
    $$PWD/qwaylandxcompositeeglwindow.h
