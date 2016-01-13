PLUGIN_TYPE = waylandcompositors
load(qt_plugin)

QT = compositor compositor-private core-private gui-private

INCLUDEPATH += $$PWD

!contains(QT_CONFIG, no-pkg-config) {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-server
} else {
    LIBS += -lwayland-server
}

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
