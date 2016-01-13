TARGET  = QtCompositor
QT = core gui-private

contains(QT_CONFIG, opengl):MODULE_DEFINES = QT_COMPOSITOR_WAYLAND_GL

MODULE_PLUGIN_TYPES = wayland-graphics-integration-server
load(qt_module)

CONFIG -= precompile_header
CONFIG += link_pkgconfig

DEFINES += QT_WAYLAND_WINDOWMANAGER_SUPPORT

!contains(QT_CONFIG, no-pkg-config) {
    PKGCONFIG_PRIVATE += wayland-server
} else {
    LIBS += -lwayland-server
}

INCLUDEPATH += ../shared
HEADERS += ../shared/qwaylandmimehelper.h
SOURCES += ../shared/qwaylandmimehelper.cpp

include ($$PWD/global/global.pri)
include ($$PWD/wayland_wrapper/wayland_wrapper.pri)
include ($$PWD/hardware_integration/hardware_integration.pri)
include ($$PWD/compositor_api/compositor_api.pri)
include ($$PWD/windowmanagerprotocol/windowmanagerprotocol.pri)


