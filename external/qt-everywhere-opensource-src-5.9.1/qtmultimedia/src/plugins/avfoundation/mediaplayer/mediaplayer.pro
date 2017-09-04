TARGET = qavfmediaplayer

#DEFINES += QT_DEBUG_AVF
# Avoid clash with a variable named `slots' in a Quartz header
CONFIG += no_keywords

QT += multimedia-private network

LIBS += -framework CoreMedia -framework CoreVideo -framework QuartzCore

QMAKE_USE += avfoundation

HEADERS += \
    avfmediaplayercontrol.h \
    avfmediaplayermetadatacontrol.h \
    avfmediaplayerservice.h \
    avfmediaplayersession.h \
    avfmediaplayerserviceplugin.h \
    avfvideooutput.h \
    avfvideowindowcontrol.h

OBJECTIVE_SOURCES += \
    avfmediaplayercontrol.mm \
    avfmediaplayermetadatacontrol.mm \
    avfmediaplayerservice.mm \
    avfmediaplayerserviceplugin.mm \
    avfmediaplayersession.mm \
    avfvideooutput.mm \
    avfvideowindowcontrol.mm

    qtHaveModule(widgets) {
        QT += multimediawidgets-private
        HEADERS += \
            avfvideowidgetcontrol.h \
            avfvideowidget.h

        OBJECTIVE_SOURCES += \
            avfvideowidgetcontrol.mm \
            avfvideowidget.mm
    }

ios|tvos {
    qtConfig(opengl) {
        HEADERS += \
            avfvideoframerenderer_ios.h \
            avfvideorenderercontrol.h \
            avfdisplaylink.h

        OBJECTIVE_SOURCES += \
            avfvideoframerenderer_ios.mm \
            avfvideorenderercontrol.mm \
            avfdisplaylink.mm
    }
    LIBS += -framework Foundation
} else {
    LIBS += -framework AppKit

    qtConfig(opengl) {
        HEADERS += \
            avfvideoframerenderer.h \
            avfvideorenderercontrol.h \
            avfdisplaylink.h

        OBJECTIVE_SOURCES += \
            avfvideoframerenderer.mm \
            avfvideorenderercontrol.mm \
            avfdisplaylink.mm
    }
}

OTHER_FILES += \
    avfmediaplayer.json

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = AVFMediaPlayerServicePlugin
load(qt_plugin)
