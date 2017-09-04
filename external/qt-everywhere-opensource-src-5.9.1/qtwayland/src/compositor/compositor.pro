TARGET = QtWaylandCompositor
MODULE = waylandcompositor

QT = core gui-private

qtHaveModule(quick): QT += quick

CONFIG -= precompile_header
CONFIG += link_pkgconfig

QMAKE_DOCS = $$PWD/doc/qtwaylandcompositor.qdocconf

QMAKE_USE += wayland-server

INCLUDEPATH += ../shared

HEADERS += ../shared/qwaylandmimehelper_p.h \
           ../shared/qwaylandinputmethodeventbuilder_p.h \
           ../shared/qwaylandsharedmemoryformathelper_p.h

SOURCES += ../shared/qwaylandmimehelper.cpp \
           ../shared/qwaylandinputmethodeventbuilder.cpp

RESOURCES += compositor.qrc

include ($$PWD/global/global.pri)
include ($$PWD/wayland_wrapper/wayland_wrapper.pri)
include ($$PWD/hardware_integration/hardware_integration.pri)
include ($$PWD/compositor_api/compositor_api.pri)
include ($$PWD/extensions/extensions.pri)

MODULE_PLUGIN_TYPES = \
    wayland-graphics-integration-server
load(qt_module)
