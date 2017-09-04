TARGET = QtLocation
QT = core-private positioning-private
android {
    # adding qtconcurrent dependency here for the osm plugin
    QT += concurrent
}

CONFIG += simd optimize_full

# 3rdparty headers produce warnings with MSVC
msvc: CONFIG -= warning_clean

INCLUDEPATH += ../3rdparty/poly2tri
INCLUDEPATH += ../3rdparty/clipper
INCLUDEPATH += ../3rdparty/clip2tri
INCLUDEPATH += ../positioning
INCLUDEPATH += ../imports/positioning
INCLUDEPATH *= $$PWD

MODULE_PLUGIN_TYPES = \
    geoservices

QMAKE_DOCS = $$PWD/doc/qtlocation.qdocconf
OTHER_FILES += doc/src/*.qdoc   # show .qdoc files in Qt Creator

PUBLIC_HEADERS += \
                    qlocation.h \
                    qlocationglobal_p.h \
                    qlocationglobal.h

PRIVATE_HEADERS += \
                    qlocationglobal_p.h

SOURCES += \
           qlocation.cpp

include(maps/maps.pri)
include(places/places.pri)
include(declarativemaps/declarativemaps.pri)
include(declarativeplaces/declarativeplaces.pri)

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

load(qt_module)

LIBS_PRIVATE += -L$$MODULE_BASE_OUTDIR/lib -lclip2tri$$qtPlatformTargetSuffix()
