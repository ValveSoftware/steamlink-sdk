TARGET = QtWaylandClient
MODULE = waylandclient

QT += core-private gui-private
QT_FOR_PRIVATE += service_support-private
QT_PRIVATE += fontdatabase_support-private eventdispatcher_support-private theme_support-private

# We have a bunch of C code with casts, so we can't have this option
QMAKE_CXXFLAGS_WARN_ON -= -Wcast-qual

# Prevent gold linker from crashing.
# This started happening when QtPlatformSupport was modularized.
use_gold_linker: CONFIG += no_linker_version_script

CONFIG -= precompile_header
CONFIG += link_pkgconfig wayland-scanner

qtConfig(xkbcommon-evdev): \
    QMAKE_USE_PRIVATE += xkbcommon_evdev

QMAKE_USE += wayland-client

INCLUDEPATH += $$PWD/../shared

WAYLANDCLIENTSOURCES += \
            ../extensions/surface-extension.xml \
            ../extensions/touch-extension.xml \
            ../extensions/qtkey-extension.xml \
            ../extensions/qt-windowmanager.xml \
            ../3rdparty/protocol/text-input-unstable-v2.xml \
            ../3rdparty/protocol/xdg-shell.xml \
            ../3rdparty/protocol/xdg-shell-unstable-v6.xml \

WAYLANDCLIENTSOURCES_SYSTEM += \
            ../3rdparty/protocol/wayland.xml \

SOURCES +=  qwaylandintegration.cpp \
            qwaylandnativeinterface.cpp \
            qwaylandshmbackingstore.cpp \
            qwaylandinputdevice.cpp \
            qwaylanddisplay.cpp \
            qwaylandwindow.cpp \
            qwaylandscreen.cpp \
            qwaylandshmwindow.cpp \
            qwaylandshellsurface.cpp \
            qwaylandwlshellsurface.cpp \
            qwaylandwlshellintegration.cpp \
            qwaylandxdgshell.cpp \
            qwaylandxdgsurface.cpp \
            qwaylandxdgpopup.cpp \
            qwaylandxdgshellintegration.cpp \
            qwaylandxdgshellv6.cpp \
            qwaylandxdgshellv6integration.cpp \
            qwaylandextendedsurface.cpp \
            qwaylandsubsurface.cpp \
            qwaylandtouch.cpp \
            qwaylandqtkey.cpp \
            ../shared/qwaylandmimehelper.cpp \
            ../shared/qwaylandxkb.cpp \
            ../shared/qwaylandinputmethodeventbuilder.cpp \
            qwaylandabstractdecoration.cpp \
            qwaylanddecorationfactory.cpp \
            qwaylanddecorationplugin.cpp \
            qwaylandwindowmanagerintegration.cpp \
            qwaylandinputcontext.cpp \
            qwaylandshm.cpp \
            qwaylandbuffer.cpp \

HEADERS +=  qwaylandintegration_p.h \
            qwaylandnativeinterface_p.h \
            qwaylanddisplay_p.h \
            qwaylandwindow_p.h \
            qwaylandscreen_p.h \
            qwaylandshmbackingstore_p.h \
            qwaylandinputdevice_p.h \
            qwaylandbuffer_p.h \
            qwaylandshmwindow_p.h \
            qwaylandshellsurface_p.h \
            qwaylandwlshellsurface_p.h \
            qwaylandwlshellintegration_p.h \
            qwaylandxdgshell_p.h \
            qwaylandxdgsurface_p.h \
            qwaylandxdgpopup_p.h \
            qwaylandxdgshellintegration_p.h \
            qwaylandxdgshellv6_p.h \
            qwaylandxdgshellv6integration_p.h \
            qwaylandextendedsurface_p.h \
            qwaylandsubsurface_p.h \
            qwaylandtouch_p.h \
            qwaylandqtkey_p.h \
            qwaylandabstractdecoration_p.h \
            qwaylanddecorationfactory_p.h \
            qwaylanddecorationplugin_p.h \
            qwaylandwindowmanagerintegration_p.h \
            qwaylandinputcontext_p.h \
            qwaylandshm_p.h \
            qtwaylandclientglobal.h \
            qtwaylandclientglobal_p.h \
            ../shared/qwaylandinputmethodeventbuilder_p.h \
            ../shared/qwaylandmimehelper_p.h \
            ../shared/qwaylandxkb_p.h \
            ../shared/qwaylandsharedmemoryformathelper_p.h \

qtConfig(clipboard) {
    HEADERS += qwaylandclipboard_p.h
    SOURCES += qwaylandclipboard.cpp
}

include(hardwareintegration/hardwareintegration.pri)
include(shellintegration/shellintegration.pri)
include(inputdeviceintegration/inputdeviceintegration.pri)
include(global/global.pri)

qtConfig(cursor) {
    QMAKE_USE += wayland-cursor

    HEADERS += \
        qwaylandcursor_p.h
    SOURCES += \
        qwaylandcursor.cpp
}

qtConfig(wayland-datadevice) {
    HEADERS += \
        qwaylanddatadevice_p.h \
        qwaylanddatadevicemanager_p.h \
        qwaylanddataoffer_p.h \
        qwaylanddatasource_p.h
    SOURCES += \
        qwaylanddatadevice.cpp \
        qwaylanddatadevicemanager.cpp \
        qwaylanddataoffer.cpp \
        qwaylanddatasource.cpp
}

qtConfig(draganddrop) {
    HEADERS += \
        qwaylanddnd_p.h
    SOURCES += \
        qwaylanddnd.cpp
}

CONFIG += generated_privates
MODULE_PLUGIN_TYPES = \
            wayland-graphics-integration-client \
            wayland-inputdevice-integration \
            wayland-decoration-client \
            wayland-shell-integration
load(qt_module)
