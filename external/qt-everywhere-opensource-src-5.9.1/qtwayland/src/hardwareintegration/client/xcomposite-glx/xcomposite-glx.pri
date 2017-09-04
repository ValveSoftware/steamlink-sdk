INCLUDEPATH += $$PWD
include ($$PWD/../xcomposite_share/xcomposite_share.pri)

QMAKE_USE += wayland-client glx libdl

QT += glx_support-private

SOURCES += \
    $$PWD/qwaylandxcompositeglxcontext.cpp \
    $$PWD/qwaylandxcompositeglxintegration.cpp \
    $$PWD/qwaylandxcompositeglxwindow.cpp

HEADERS += \
    $$PWD/qwaylandxcompositeglxcontext.h \
    $$PWD/qwaylandxcompositeglxintegration.h \
    $$PWD/qwaylandxcompositeglxwindow.h
