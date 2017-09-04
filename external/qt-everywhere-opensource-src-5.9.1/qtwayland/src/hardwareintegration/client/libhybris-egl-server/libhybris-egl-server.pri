INCLUDEPATH += $$PWD

QMAKE_USE += egl wayland-client

SOURCES += \
        $$PWD/libhybriseglserverbufferintegration.cpp

HEADERS += \
        $$PWD/libhybriseglserverbufferintegration.h

CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += $$PWD/../../../extensions/libhybris-egl-server-buffer.xml
