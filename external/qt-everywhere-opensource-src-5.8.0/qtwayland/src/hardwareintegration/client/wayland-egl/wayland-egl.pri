INCLUDEPATH += $$PWD

QMAKE_USE += egl wayland-client wayland-egl

QT += egl_support-private

SOURCES += $$PWD/qwaylandeglclientbufferintegration.cpp \
           $$PWD/qwaylandglcontext.cpp \
           $$PWD/qwaylandeglwindow.cpp

HEADERS += $$PWD/qwaylandeglclientbufferintegration.h \
           $$PWD/qwaylandglcontext.h \
           $$PWD/qwaylandeglwindow.h \
           $$PWD/qwaylandeglinclude.h
