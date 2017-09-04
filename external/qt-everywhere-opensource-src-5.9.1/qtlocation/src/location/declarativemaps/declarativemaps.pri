QT += quick-private network positioning-private qml-private core-private gui-private

INCLUDEPATH += declarativemaps

PRIVATE_HEADERS += \
           declarativemaps/error_messages_p.h \
           declarativemaps/qdeclarativegeomapitemview_p.h \
           declarativemaps/qdeclarativegeomapitemview_p_p.h \
           declarativemaps/qdeclarativegeoserviceprovider_p.h \
           declarativemaps/qdeclarativegeocodemodel_p.h \
           declarativemaps/qdeclarativegeoroutemodel_p.h \
           declarativemaps/qdeclarativegeoroute_p.h \
           declarativemaps/qdeclarativegeoroutesegment_p.h \
           declarativemaps/qdeclarativegeomaneuver_p.h \
           declarativemaps/qdeclarativegeomap_p.h \
           declarativemaps/qdeclarativegeomaptype_p.h \
           declarativemaps/qdeclarativegeomapitembase_p.h \
           declarativemaps/qdeclarativegeomapquickitem_p.h \
           declarativemaps/qdeclarativecirclemapitem_p.h \
           declarativemaps/qdeclarativerectanglemapitem_p.h \
           declarativemaps/qdeclarativepolygonmapitem_p.h \
           declarativemaps/qdeclarativepolylinemapitem_p.h \
           declarativemaps/qdeclarativeroutemapitem_p.h \
           declarativemaps/qdeclarativegeomapparameter_p.h \
           declarativemaps/qgeomapitemgeometry_p.h \
           declarativemaps/qdeclarativegeomapcopyrightsnotice_p.h \
           declarativemaps/locationvaluetypehelper_p.h \
           declarativemaps/qquickgeomapgesturearea_p.h \
           declarativemaps/qdeclarativegeomapitemgroup_p.h \
           declarativemaps/mapitemviewdelegateincubator_p.h \
           ../imports/positioning/qquickgeocoordinateanimation_p.h

SOURCES += \
           declarativemaps/qdeclarativegeomapitemview.cpp \
           declarativemaps/qdeclarativegeoserviceprovider.cpp \
           declarativemaps/qdeclarativegeocodemodel.cpp \
           declarativemaps/qdeclarativegeoroutemodel.cpp \
           declarativemaps/qdeclarativegeoroute.cpp \
           declarativemaps/qdeclarativegeoroutesegment.cpp \
           declarativemaps/qdeclarativegeomaneuver.cpp \
           declarativemaps/qdeclarativegeomap.cpp \
           declarativemaps/qdeclarativegeomaptype.cpp \
           declarativemaps/qdeclarativegeomapitembase.cpp \
           declarativemaps/qdeclarativegeomapquickitem.cpp \
           declarativemaps/qdeclarativecirclemapitem.cpp \
           declarativemaps/qdeclarativerectanglemapitem.cpp \
           declarativemaps/qdeclarativepolygonmapitem.cpp \
           declarativemaps/qdeclarativepolylinemapitem.cpp \
           declarativemaps/qdeclarativeroutemapitem.cpp \
           declarativemaps/qdeclarativegeomapparameter.cpp \
           declarativemaps/qgeomapitemgeometry.cpp \
           declarativemaps/qdeclarativegeomapcopyrightsnotice.cpp \
           declarativemaps/error_messages.cpp \
           declarativemaps/locationvaluetypehelper.cpp \
           declarativemaps/qquickgeomapgesturearea.cpp \
           declarativemaps/qdeclarativegeomapitemgroup.cpp \
           ../imports/positioning/qquickgeocoordinateanimation.cpp \
           declarativemaps/mapitemviewdelegateincubator.cpp

load(qt_build_paths)
LIBS_PRIVATE += -L$$MODULE_BASE_OUTDIR/lib -lpoly2tri$$qtPlatformTargetSuffix() -lclip2tri$$qtPlatformTargetSuffix()


