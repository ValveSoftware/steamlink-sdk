INCLUDEPATH += $$PWD

QMAKE_USE += directshow
LIBS += -lgdi32

qtHaveModule(widgets): QT += widgets

DEFINES += QMEDIA_DIRECTSHOW_PLAYER

HEADERS += \
        $$PWD/directshowioreader.h \
        $$PWD/directshowiosource.h \
        $$PWD/directshowplayercontrol.h \
        $$PWD/directshowplayerservice.h \
        $$PWD/directshowvideorenderercontrol.h \
        $$PWD/videosurfacefilter.h \
        $$PWD/directshowaudioendpointcontrol.h \
        $$PWD/directshowmetadatacontrol.h \
        $$PWD/vmr9videowindowcontrol.h

SOURCES += \
        $$PWD/directshowioreader.cpp \
        $$PWD/directshowiosource.cpp \
        $$PWD/directshowplayercontrol.cpp \
        $$PWD/directshowplayerservice.cpp \
        $$PWD/directshowvideorenderercontrol.cpp \
        $$PWD/videosurfacefilter.cpp \
        $$PWD/directshowaudioendpointcontrol.cpp \
        $$PWD/directshowmetadatacontrol.cpp \
        $$PWD/vmr9videowindowcontrol.cpp

qtConfig(evr) {
    DEFINES += HAVE_EVR

    include($$PWD/../../common/evr.pri)

    HEADERS += \
        $$PWD/directshowevrvideowindowcontrol.h

    SOURCES += \
        $$PWD/directshowevrvideowindowcontrol.cpp
}

qtConfig(wshellitem) {
    QT += core-private
} else {
    DEFINES += QT_NO_SHELLITEM
}
