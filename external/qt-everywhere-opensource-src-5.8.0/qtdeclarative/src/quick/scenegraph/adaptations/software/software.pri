QT += gui-private core-private qml-private

#DEFINES += QTQUICK2D_DEBUG_FLUSH

SOURCES += \
    $$PWD/qsgsoftwarecontext.cpp \
    $$PWD/qsgabstractsoftwarerenderer.cpp \
    $$PWD/qsgsoftwareglyphnode.cpp \
    $$PWD/qsgsoftwareinternalimagenode.cpp \
    $$PWD/qsgsoftwarepublicnodes.cpp \
    $$PWD/qsgsoftwarepainternode.cpp \
    $$PWD/qsgsoftwareinternalrectanglenode.cpp \
    $$PWD/qsgsoftwarepixmaprenderer.cpp \
    $$PWD/qsgsoftwarepixmaptexture.cpp \
    $$PWD/qsgsoftwarerenderablenode.cpp \
    $$PWD/qsgsoftwarerenderablenodeupdater.cpp \
    $$PWD/qsgsoftwarerenderer.cpp \
    $$PWD/qsgsoftwarerenderlistbuilder.cpp \
    $$PWD/qsgsoftwarerenderloop.cpp \
    $$PWD/qsgsoftwarelayer.cpp \
    $$PWD/qsgsoftwareadaptation.cpp \
    $$PWD/qsgsoftwarethreadedrenderloop.cpp

HEADERS += \
    $$PWD/qsgsoftwarecontext_p.h \
    $$PWD/qsgabstractsoftwarerenderer_p.h \
    $$PWD/qsgsoftwareglyphnode_p.h \
    $$PWD/qsgsoftwareinternalimagenode_p.h \
    $$PWD/qsgsoftwarepublicnodes_p.h \
    $$PWD/qsgsoftwarepainternode_p.h \
    $$PWD/qsgsoftwarepixmaprenderer_p.h \
    $$PWD/qsgsoftwarepixmaptexture_p.h \
    $$PWD/qsgsoftwareinternalrectanglenode_p.h \
    $$PWD/qsgsoftwarerenderablenode_p.h \
    $$PWD/qsgsoftwarerenderablenodeupdater_p.h \
    $$PWD/qsgsoftwarerenderer_p.h \
    $$PWD/qsgsoftwarerenderlistbuilder_p.h \
    $$PWD/qsgsoftwarerenderloop_p.h \
    $$PWD/qsgsoftwarelayer_p.h \
    $$PWD/qsgsoftwareadaptation_p.h \
    $$PWD/qsgsoftwarethreadedrenderloop_p.h

qtConfig(quick-sprite) {
    SOURCES += \
        $$PWD/qsgsoftwarespritenode.cpp
    HEADERS += \
        $$PWD/qsgsoftwarespritenode_p.h
}
