HEADERS += $$PWD/qabstract3dgraph_p.h \
           $$PWD/qabstract3dgraph.h \
           $$PWD/q3dbars.h \
           $$PWD/q3dbars_p.h \
           $$PWD/drawer_p.h \
           $$PWD/bars3dcontroller_p.h \
           $$PWD/bars3drenderer_p.h \
           $$PWD/q3dsurface.h \
           $$PWD/q3dsurface_p.h \
           $$PWD/surface3dcontroller_p.h \
           $$PWD/surface3drenderer_p.h \
           $$PWD/abstract3dcontroller_p.h \
           $$PWD/q3dscatter.h \
           $$PWD/q3dscatter_p.h \
           $$PWD/scatter3dcontroller_p.h \
           $$PWD/scatter3drenderer_p.h \
           $$PWD/axisrendercache_p.h \
           $$PWD/seriesrendercache_p.h \
           $$PWD/abstract3drenderer_p.h \
           $$PWD/selectionpointer_p.h \
           $$PWD/q3dcamera.h \
           $$PWD/q3dcamera_p.h \
           $$PWD/q3dscene.h \
           $$PWD/q3dlight.h \
           $$PWD/q3dlight_p.h \
           $$PWD/q3dobject.h \
           $$PWD/q3dobject_p.h \
           $$PWD/q3dscene_p.h \
           $$PWD/surfaceseriesrendercache_p.h \
           $$PWD/barseriesrendercache_p.h \
           $$PWD/scatterseriesrendercache_p.h

SOURCES += $$PWD/qabstract3dgraph.cpp \
           $$PWD/q3dbars.cpp \
           $$PWD/drawer.cpp \
           $$PWD/bars3dcontroller.cpp \
           $$PWD/bars3drenderer.cpp \
           $$PWD/q3dsurface.cpp \
           $$PWD/surface3drenderer.cpp \
           $$PWD/surface3dcontroller.cpp \
           $$PWD/abstract3dcontroller.cpp \
           $$PWD/q3dscatter.cpp \
           $$PWD/scatter3dcontroller.cpp \
           $$PWD/scatter3drenderer.cpp \
           $$PWD/axisrendercache.cpp \
           $$PWD/seriesrendercache.cpp \
           $$PWD/abstract3drenderer.cpp \
           $$PWD/selectionpointer.cpp \
           $$PWD/q3dcamera.cpp \
           $$PWD/q3dlight.cpp \
           $$PWD/q3dobject.cpp \
           $$PWD/q3dscene.cpp \
           $$PWD/surfaceseriesrendercache.cpp \
           $$PWD/barseriesrendercache.cpp \
           $$PWD/scatterseriesrendercache.cpp

RESOURCES += engine/engine.qrc

OTHER_FILES += $$PWD/meshes/* $$PWD/shaders/*

INCLUDEPATH += $$PWD
