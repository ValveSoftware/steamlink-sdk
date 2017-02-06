QT += openglextensions qml quick qml-private core-private
DEFINES += QTCANVAS3D_LIBRARY
TARGETPATH = QtCanvas3D
IMPORT_VERSION = 1.1

QMAKE_DOCS = $$PWD/doc/qtcanvas3d.qdocconf

SOURCES += arrayutils.cpp \
    qcanvas3d_plugin.cpp \
    enumtostringmap.cpp \
    abstractobject3d.cpp \
    canvas3d.cpp \
    buffer3d.cpp \
    canvasrendernode.cpp \
    context3d.cpp \
    contextattributes.cpp \
    framebuffer3d.cpp \
    program3d.cpp \
    renderbuffer3d.cpp \
    shader3d.cpp \
    shaderprecisionformat.cpp \
    teximage3d.cpp \
    texture3d.cpp \
    uniformlocation.cpp \
    activeinfo3d.cpp \
    canvasglstatedump.cpp \
    compressedtextures3tc.cpp \
    compressedtexturepvrtc.cpp \
    glcommandqueue.cpp \
    renderjob.cpp \
    canvasrenderer.cpp \
    canvastextureprovider.cpp \
    glstatestore.cpp \
    openglversionchecker.cpp

HEADERS += arrayutils_p.h \
    qcanvas3d_plugin.h \
    enumtostringmap_p.h \
    abstractobject3d_p.h \
    canvas3d_p.h \
    buffer3d_p.h \
    canvas3dcommon_p.h \
    canvasrendernode_p.h \
    context3d_p.h \
    contextattributes_p.h \
    framebuffer3d_p.h \
    program3d_p.h \
    renderbuffer3d_p.h \
    shader3d_p.h \
    shaderprecisionformat_p.h \
    teximage3d_p.h \
    texture3d_p.h \
    uniformlocation_p.h \
    activeinfo3d_p.h \
    canvasglstatedump_p.h \
    compressedtextures3tc_p.h \
    compressedtexturepvrtc_p.h \
    glcommandqueue_p.h \
    renderjob_p.h \
    canvasrenderer_p.h \
    canvastextureprovider_p.h \
    glstatestore_p.h \
    openglversionchecker_p.h

OTHER_FILES = qmldir \
    doc/* \
    doc/src/* \
    doc/images/* \
    doc/snippets/*

CONFIG += no_cxx_module

load(qml_plugin)
