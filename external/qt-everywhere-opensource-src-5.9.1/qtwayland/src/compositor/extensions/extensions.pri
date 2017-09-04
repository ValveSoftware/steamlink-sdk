CONFIG += wayland-scanner
CONFIG += generated_privates

WAYLANDSERVERSOURCES += \
    ../extensions/surface-extension.xml \
    ../extensions/touch-extension.xml \
    ../extensions/qtkey-extension.xml \
    ../extensions/qt-windowmanager.xml \
    ../3rdparty/protocol/text-input-unstable-v2.xml \
    ../3rdparty/protocol/xdg-shell.xml \
    ../3rdparty/protocol/ivi-application.xml \

HEADERS += \
    extensions/qwlextendedsurface_p.h \
    extensions/qwlqttouch_p.h \
    extensions/qwlqtkey_p.h \
    extensions/qwaylandshell.h \
    extensions/qwaylandshell_p.h \
    extensions/qwaylandwlshell.h \
    extensions/qwaylandwlshell_p.h \
    extensions/qwaylandtextinput.h \
    extensions/qwaylandtextinput_p.h \
    extensions/qwaylandtextinputmanager.h \
    extensions/qwaylandtextinputmanager_p.h \
    extensions/qwaylandqtwindowmanager.h \
    extensions/qwaylandqtwindowmanager_p.h \
    extensions/qwaylandxdgshellv5.h \
    extensions/qwaylandxdgshellv5_p.h \
    extensions/qwaylandshellsurface.h \
    extensions/qwaylandiviapplication.h \
    extensions/qwaylandiviapplication_p.h \
    extensions/qwaylandivisurface.h \
    extensions/qwaylandivisurface_p.h \

SOURCES += \
    extensions/qwlextendedsurface.cpp \
    extensions/qwlqttouch.cpp \
    extensions/qwlqtkey.cpp \
    extensions/qwaylandshell.cpp \
    extensions/qwaylandwlshell.cpp \
    extensions/qwaylandtextinput.cpp \
    extensions/qwaylandtextinputmanager.cpp \
    extensions/qwaylandqtwindowmanager.cpp \
    extensions/qwaylandxdgshellv5.cpp \
    extensions/qwaylandiviapplication.cpp \
    extensions/qwaylandivisurface.cpp \

qtHaveModule(quick):contains(QT_CONFIG, opengl) {
    HEADERS += \
        extensions/qwaylandquickshellsurfaceitem.h \
        extensions/qwaylandquickshellsurfaceitem_p.h \
        extensions/qwaylandivisurfaceintegration_p.h \
        extensions/qwaylandwlshellintegration_p.h \
        extensions/qwaylandxdgshellv5integration_p.h \

    SOURCES += \
        extensions/qwaylandquickshellsurfaceitem.cpp \
        extensions/qwaylandivisurfaceintegration.cpp \
        extensions/qwaylandwlshellintegration.cpp \
        extensions/qwaylandxdgshellv5integration.cpp \

}

INCLUDEPATH += extensions
