QT += quick-private network positioning-private location-private qml-private core-private gui-private

INCLUDEPATH += ../../location
INCLUDEPATH += ../../location/maps
INCLUDEPATH += ../../positioning
INCLUDEPATH += ../positioning
INCLUDEPATH *= $$PWD

HEADERS += \
           qdeclarativegeomapitemview_p.h \
           qdeclarativegeoserviceprovider_p.h \
           qdeclarativegeocodemodel_p.h \
           qdeclarativegeoroutemodel_p.h \
           qdeclarativegeoroute_p.h \
           qdeclarativegeoroutesegment_p.h \
           qdeclarativegeomaneuver_p.h \
           qdeclarativegeomap_p.h \
           qdeclarativegeomaptype_p.h \
           qdeclarativegeomapitembase_p.h \
           qdeclarativegeomapquickitem_p.h \
           qdeclarativecirclemapitem_p.h \
           qdeclarativerectanglemapitem_p.h \
           qdeclarativepolygonmapitem_p.h \
           qdeclarativepolylinemapitem_p.h \
           qdeclarativeroutemapitem_p.h \
           qgeomapitemgeometry_p.h \
           qdeclarativegeomapcopyrightsnotice_p.h \
           error_messages.h \
           locationvaluetypehelper_p.h\
           qquickgeomapgesturearea_p.h\
           ../positioning/qquickgeocoordinateanimation_p.h \
           mapitemviewdelegateincubator.h \
           qdeclarativegeomapitemview_p_p.h

SOURCES += \
           location.cpp \
           qdeclarativegeomapitemview.cpp \
           qdeclarativegeoserviceprovider.cpp \
           qdeclarativegeocodemodel.cpp \
           qdeclarativegeoroutemodel.cpp \
           qdeclarativegeoroute.cpp \
           qdeclarativegeoroutesegment.cpp \
           qdeclarativegeomaneuver.cpp \
           qdeclarativegeomap.cpp \
           qdeclarativegeomaptype.cpp \
           qdeclarativegeomapitembase.cpp \
           qdeclarativegeomapquickitem.cpp \
           qdeclarativecirclemapitem.cpp \
           qdeclarativerectanglemapitem.cpp \
           qdeclarativepolygonmapitem.cpp \
           qdeclarativepolylinemapitem.cpp \
           qdeclarativeroutemapitem.cpp \
           qgeomapitemgeometry.cpp \
           qdeclarativegeomapcopyrightsnotice.cpp \
           error_messages.cpp \
           locationvaluetypehelper.cpp \
           qquickgeomapgesturearea.cpp \
           ../positioning/qquickgeocoordinateanimation.cpp \
           mapitemviewdelegateincubator.cpp

include(declarativeplaces/declarativeplaces.pri)

load(qml_plugin)

LIBS_PRIVATE += -L$$MODULE_BASE_OUTDIR/lib -lpoly2tri$$qtPlatformTargetSuffix() -lclip2tri$$qtPlatformTargetSuffix()

OTHER_FILES += \
    plugin.json \
    qmldir
