INCLUDEPATH += $$PWD

QMAKE_USE_PRIVATE += egl wayland-server

SOURCES += \
    $$PWD/drmeglserverbufferintegration.cpp


HEADERS += \
    $$PWD/drmeglserverbufferintegration.h

CONFIG += wayland-scanner
WAYLANDSERVERSOURCES += $$PWD/../../../extensions/drm-egl-server-buffer.xml
