include($$PWD/../xcomposite_share/xcomposite_share.pri)

QMAKE_USE_PRIVATE += egl wayland-server x11

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/xcompositeeglintegration.h

SOURCES += \
    $$PWD/xcompositeeglintegration.cpp
