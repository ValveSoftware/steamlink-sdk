# distinct from Qt Multimedia
TARGET = QtMultimediaWidgets
QT = core gui multimedia-private widgets-private
qtHaveModule(opengl):!contains(QT_CONFIG, opengles1) {
   QT_PRIVATE += opengl
} else {
   DEFINES += QT_NO_OPENGL
}

QMAKE_DOCS = $$PWD/doc/qtmultimediawidgets.qdocconf

load(qt_module)

PRIVATE_HEADERS += \
    qvideowidget_p.h \
    qpaintervideosurface_p.h \

PUBLIC_HEADERS += \
    qtmultimediawidgetdefs.h \
    qcameraviewfinder.h \
    qgraphicsvideoitem.h \
    qvideowidgetcontrol.h \
    qvideowidget.h

SOURCES += \
    qcameraviewfinder.cpp \
    qpaintervideosurface.cpp \
    qvideowidgetcontrol.cpp \
    qvideowidget.cpp

maemo6 {
    contains(QT_CONFIG, opengles2) {
        PRIVATE_HEADERS += qeglimagetexturesurface_p.h
        SOURCES += qeglimagetexturesurface.cpp

        SOURCES += qgraphicsvideoitem_maemo6.cpp

        LIBS_PRIVATE += -lX11
    } else {
        SOURCES += qgraphicsvideoitem.cpp
    }
}

!maemo* {
    SOURCES += qgraphicsvideoitem.cpp
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS
