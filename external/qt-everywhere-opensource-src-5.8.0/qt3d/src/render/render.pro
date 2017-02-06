TARGET   = Qt3DRender
MODULE   = 3drender

QT = core-private gui-private 3dcore-private
QT_PRIVATE = openglextensions
QT_FOR_PRIVATE = concurrent

include (backend/render-backend.pri)
include (geometry/geometry.pri)
include (graphicshelpers/graphicshelpers.pri)
include (framegraph/framegraph.pri)
include (frontend/render-frontend.pri)
include (jobs/jobs.pri)
include (lights/lights.pri)
include (materialsystem/materialsystem.pri)
include (renderstates/renderstates.pri)
include (io/io.pri)
include (picking/picking.pri)
include (raycasting/raycasting.pri)
include (services/services.pri)
include (texture/texture.pri)

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

gcov {
    QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
    QMAKE_LFLAGS += -fprofile-arcs -ftest-coverage
}

HEADERS += \
    renderlogging_p.h \
    qt3drender_global.h \
    qt3drender_global_p.h

# otherwise mingw headers do not declare common functions like ::strcasecmp
win32-g++*:QMAKE_CXXFLAGS_CXX11 = -std=gnu++0x

SOURCES += \
    renderlogging.cpp

MODULE_PLUGIN_TYPES = \
    sceneparsers
load(qt_module)
