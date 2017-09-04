TARGET = qtgeoservices_mapboxgl

QT += \
    quick-private \
    location-private \
    positioning-private \
    network \
    sql

HEADERS += \
    qgeoserviceproviderpluginmapboxgl.h \
    qgeomappingmanagerenginemapboxgl.h \
    qgeomapmapboxgl.h \
    qgeomapmapboxgl_p.h \
    qmapboxglstylechange_p.h \
    qsgmapboxglnode.h

SOURCES += \
    qgeoserviceproviderpluginmapboxgl.cpp \
    qgeomappingmanagerenginemapboxgl.cpp \
    qgeomapmapboxgl.cpp \
    qmapboxglstylechange.cpp \
    qsgmapboxglnode.cpp

RESOURCES += mapboxgl.qrc

OTHER_FILES += \
    mapboxgl_plugin.json

INCLUDEPATH += ../../../3rdparty/mapbox-gl-native/platform/qt/include

include(../../../3rdparty/zlib_dependency.pri)

load(qt_build_paths)
LIBS_PRIVATE += -L$$MODULE_BASE_OUTDIR/lib -lqmapboxgl$$qtPlatformTargetSuffix()

qtConfig(icu) {
    include(../../../3rdparty/icu_dependency.pri)
}

# When building for Windows with dynamic OpenGL, this plugin
# can only run with ANGLE because Mapbox GL requires at least
# OpenGL ES and does not use QOpenGLFunctions for resolving
# the OpenGL symbols. -lopengl32 only gives OpenGL 1.1.
win32:qtConfig(dynamicgl) {
    qtConfig(combined-angle-lib): LIBS_PRIVATE += -l$${LIBQTANGLE_NAME}
    else: LIBS_PRIVATE += -l$${LIBEGL_NAME} -l$${LIBGLESV2_NAME}
}

PLUGIN_TYPE = geoservices
PLUGIN_CLASS_NAME = QGeoServiceProviderFactoryMapboxGL
load(qt_plugin)
