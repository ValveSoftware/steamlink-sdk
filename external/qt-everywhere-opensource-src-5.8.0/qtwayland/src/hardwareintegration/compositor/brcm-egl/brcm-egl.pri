QT = waylandcompositor waylandcompositor-private core-private gui-private

INCLUDEPATH += $$PWD

QMAKE_USE_PRIVATE += wayland-server

for(p, QMAKE_LIBDIR_EGL) {
    exists($$p):LIBS += -L$$p
}

LIBS += $$QMAKE_LIBS_EGL
INCLUDEPATH += $$QMAKE_INCDIR_EGL

SOURCES += \
    $$PWD/brcmeglintegration.cpp \
    $$PWD/brcmbuffer.cpp


HEADERS += \
    $$PWD/brcmeglintegration.h \
    $$PWD/brcmbuffer.h

CONFIG += wayland-scanner
WAYLANDSERVERSOURCES += $$PWD/../../../extensions/brcm.xml
