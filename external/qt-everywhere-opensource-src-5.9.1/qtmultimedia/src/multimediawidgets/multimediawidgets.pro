# distinct from Qt Multimedia
TARGET = QtMultimediaWidgets
QT = core gui multimedia-private widgets-private
qtHaveModule(opengl): \
   QT_PRIVATE += opengl

PRIVATE_HEADERS += \
    qvideowidget_p.h \
    qpaintervideosurface_p.h \

PUBLIC_HEADERS += \
    qtmultimediawidgetdefs.h \
    qcameraviewfinder.h \
    qvideowidgetcontrol.h \
    qvideowidget.h

SOURCES += \
    qcameraviewfinder.cpp \
    qpaintervideosurface.cpp \
    qvideowidgetcontrol.cpp \
    qvideowidget.cpp

qtConfig(graphicsview) {
    SOURCES        += qgraphicsvideoitem.cpp
    PUBLIC_HEADERS += qgraphicsvideoitem.h
}


HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

load(qt_module)
