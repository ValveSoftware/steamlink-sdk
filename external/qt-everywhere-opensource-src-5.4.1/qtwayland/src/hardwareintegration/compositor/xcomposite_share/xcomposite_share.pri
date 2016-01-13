INCLUDEPATH += $$PWD

CONFIG += wayland-scanner
WAYLANDSERVERSOURCES += $$PWD/../../../extensions/xcomposite.xml $$PWD/../../../3rdparty/protocol/wayland.xml

HEADERS += \
    $$PWD/xcompositebuffer.h \
    $$PWD/xcompositehandler.h \
    $$PWD/xlibinclude.h

SOURCES += \
    $$PWD/xcompositebuffer.cpp \
    $$PWD/xcompositehandler.cpp

QT += gui-private
