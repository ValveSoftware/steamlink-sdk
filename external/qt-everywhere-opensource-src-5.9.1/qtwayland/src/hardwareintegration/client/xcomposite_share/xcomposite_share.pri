INCLUDEPATH += $$PWD

QMAKE_USE += xcomposite
CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += $$PWD/../../../extensions/xcomposite.xml

HEADERS += \
    $$PWD/qwaylandxcompositebuffer.h

SOURCES += \
    $$PWD/qwaylandxcompositebuffer.cpp
