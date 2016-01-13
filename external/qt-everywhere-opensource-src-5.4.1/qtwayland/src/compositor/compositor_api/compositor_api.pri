INCLUDEPATH += compositor_api

HEADERS += \
    compositor_api/qwaylandcompositor.h \
    compositor_api/qwaylandsurface.h \
    compositor_api/qwaylandsurface_p.h \
    compositor_api/qwaylandinput.h \
    compositor_api/qwaylandinputpanel.h \
    compositor_api/qwaylanddrag.h \
    compositor_api/qwaylandbufferref.h \
    compositor_api/qwaylandsurfaceview.h \
    compositor_api/qwaylandglobalinterface.h \
    compositor_api/qwaylandsurfaceinterface.h

SOURCES += \
    compositor_api/qwaylandcompositor.cpp \
    compositor_api/qwaylandsurface.cpp \
    compositor_api/qwaylandinput.cpp \
    compositor_api/qwaylandinputpanel.cpp \
    compositor_api/qwaylanddrag.cpp \
    compositor_api/qwaylandbufferref.cpp \
    compositor_api/qwaylandsurfaceview.cpp \
    compositor_api/qwaylandglobalinterface.cpp \
    compositor_api/qwaylandsurfaceinterface.cpp

QT += core-private

qtHaveModule(quick) {
    SOURCES += \
        compositor_api/qwaylandquickcompositor.cpp \
        compositor_api/qwaylandquicksurface.cpp \
        compositor_api/qwaylandsurfaceitem.cpp

    HEADERS += \
        compositor_api/qwaylandquickcompositor.h \
        compositor_api/qwaylandquicksurface.h \
        compositor_api/qwaylandsurfaceitem.h

    QT += qml quick
}
