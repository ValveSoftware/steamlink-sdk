INCLUDEPATH += $$PWD

QMAKE_USE += egl wayland-client

SOURCES += \
        $$PWD/drmeglserverbufferintegration.cpp

HEADERS += \
        $$PWD/drmeglserverbufferintegration.h

CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += $$PWD/../../../extensions/drm-egl-server-buffer.xml
