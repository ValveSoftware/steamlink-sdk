CONFIG += testcase link_pkgconfig
CONFIG += wayland-scanner
TARGET = tst_compositor

QT += testlib
QT += core-private gui-private waylandcompositor waylandcompositor-private

QMAKE_USE += wayland-client wayland-server

qtConfig(xkbcommon-evdev)
    QMAKE_USE += xkbcommon_evdev

WAYLANDCLIENTSOURCES += \
            ../../../../src/3rdparty/protocol/xdg-shell.xml \
            ../../../../src/3rdparty/protocol/ivi-application.xml \

SOURCES += \
    tst_compositor.cpp \
    testcompositor.cpp \
    testkeyboardgrabber.cpp \
    mockclient.cpp \
    mockseat.cpp \
    testseat.cpp

HEADERS += \
    testcompositor.h \
    testkeyboardgrabber.h \
    mockclient.h \
    mockseat.h \
    testseat.h
