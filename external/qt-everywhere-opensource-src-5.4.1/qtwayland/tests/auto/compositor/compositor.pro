CONFIG += testcase link_pkgconfig
TARGET = tst_compositor

QT += testlib
QT += core-private gui-private compositor compositor-private

!contains(QT_CONFIG, no-pkg-config) {
    PKGCONFIG += wayland-client wayland-server
} else {
    LIBS += -lwayland-client -lwayland-server
}

config_xkbcommon {
    !contains(QT_CONFIG, no-pkg-config) {
        PKGCONFIG_PRIVATE += xkbcommon
    } else {
        LIBS_PRIVATE += -lxkbcommon
    }
} else {
    DEFINES += QT_NO_WAYLAND_XKB
}

SOURCES += tst_compositor.cpp \
           testcompositor.cpp \
           testkeyboardgrabber.cpp \
           mockclient.cpp \

HEADERS += testcompositor.h \
           testkeyboardgrabber.h \
           mockclient.h \
