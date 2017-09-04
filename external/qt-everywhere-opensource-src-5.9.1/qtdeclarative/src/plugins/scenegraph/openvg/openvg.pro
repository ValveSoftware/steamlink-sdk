TARGET = qsgopenvgbackend

QT += gui-private core-private qml-private quick-private

PLUGIN_TYPE = scenegraph
PLUGIN_CLASS_NAME = QSGOpenVGAdaptation
load(qt_plugin)

QMAKE_TARGET_PRODUCT = "Qt Quick OpenVG Renderer (Qt $$QT_VERSION)"
QMAKE_TARGET_DESCRIPTION = "Quick OpenVG Renderer for Qt."

QMAKE_USE += openvg
CONFIG += egl

OTHER_FILES += $$PWD/openvg.json

HEADERS += \
    qsgopenvgadaptation_p.h \
    qsgopenvgcontext_p.h \
    qsgopenvgrenderloop_p.h \
    qsgopenvgglyphnode_p.h \
    qopenvgcontext_p.h \
    qsgopenvgrenderer_p.h \
    qsgopenvginternalrectanglenode.h \
    qsgopenvgnodevisitor.h \
    qopenvgmatrix.h \
    qsgopenvgpublicnodes.h \
    qsgopenvginternalimagenode.h \
    qsgopenvgtexture.h \
    qsgopenvglayer.h \
    qsgopenvghelpers.h \
    qsgopenvgfontglyphcache.h \
    qsgopenvgpainternode.h \
    qsgopenvgrenderable.h \
    qopenvgoffscreensurface.h

SOURCES += \
    qsgopenvgadaptation.cpp \
    qsgopenvgcontext.cpp \
    qsgopenvgrenderloop.cpp \
    qsgopenvgglyphnode.cpp \
    qopenvgcontext.cpp \
    qsgopenvgrenderer.cpp \
    qsgopenvginternalrectanglenode.cpp \
    qsgopenvgnodevisitor.cpp \
    qopenvgmatrix.cpp \
    qsgopenvgpublicnodes.cpp \
    qsgopenvginternalimagenode.cpp \
    qsgopenvgtexture.cpp \
    qsgopenvglayer.cpp \
    qsgopenvghelpers.cpp \
    qsgopenvgfontglyphcache.cpp \
    qsgopenvgpainternode.cpp \
    qsgopenvgrenderable.cpp \
    qopenvgoffscreensurface.cpp

qtConfig(quick-sprite) {
    HEADERS += qsgopenvgspritenode.h
    SOURCES += qsgopenvgspritenode.cpp
}
