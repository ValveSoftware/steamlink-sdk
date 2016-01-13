INCLUDEPATH += $$PWD

DEFINES += QMEDIA_DIRECTSHOW_PLAYER

HEADERS += \
        $$PWD/directshowaudioendpointcontrol.h \
        $$PWD/directshoweventloop.h \
        $$PWD/directshowglobal.h \
        $$PWD/directshowioreader.h \
        $$PWD/directshowiosource.h \
        $$PWD/directshowmediatype.h \
        $$PWD/directshowmediatypelist.h \
        $$PWD/directshowmetadatacontrol.h \
        $$PWD/directshowpinenum.h \
        $$PWD/directshowplayercontrol.h \
        $$PWD/directshowplayerservice.h \
        $$PWD/directshowsamplescheduler.h \
        $$PWD/directshowvideorenderercontrol.h \
        $$PWD/mediasamplevideobuffer.h \
        $$PWD/videosurfacefilter.h

SOURCES += \
        $$PWD/directshowaudioendpointcontrol.cpp \
        $$PWD/directshoweventloop.cpp \
        $$PWD/directshowioreader.cpp \
        $$PWD/directshowiosource.cpp \
        $$PWD/directshowmediatype.cpp \
        $$PWD/directshowmediatypelist.cpp \
        $$PWD/directshowmetadatacontrol.cpp \
        $$PWD/directshowpinenum.cpp \
        $$PWD/directshowplayercontrol.cpp \
        $$PWD/directshowplayerservice.cpp \
        $$PWD/directshowsamplescheduler.cpp \
        $$PWD/directshowvideorenderercontrol.cpp \
        $$PWD/mediasamplevideobuffer.cpp \
        $$PWD/videosurfacefilter.cpp

qtHaveModule(widgets):!simulator {
    HEADERS += \
        $$PWD/vmr9videowindowcontrol.h

    SOURCES += \
        $$PWD/vmr9videowindowcontrol.cpp
}

config_wshellitem {
    QT += core-private
} else {
    DEFINES += QT_NO_SHELLITEM
}

LIBS += -lstrmiids -ldmoguids -luuid -lmsdmo -lole32 -loleaut32 -lgdi32

