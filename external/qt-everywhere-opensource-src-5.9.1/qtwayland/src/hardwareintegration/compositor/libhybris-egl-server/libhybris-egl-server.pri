INCLUDEPATH += $$PWD

QMAKE_USE_PRIVATE += egl wayland-server

SOURCES += \
    $$PWD/libhybriseglserverbufferintegration.cpp


HEADERS += \
    $$PWD/libhybriseglserverbufferintegration.h

CONFIG += wayland-scanner
WAYLANDSERVERSOURCES += $$PWD/../../../extensions/libhybris-egl-server-buffer.xml
