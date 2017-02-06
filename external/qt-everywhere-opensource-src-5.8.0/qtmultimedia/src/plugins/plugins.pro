######################################################################
#
# Qt Multimedia
#
######################################################################

TEMPLATE = subdirs
QT_FOR_CONFIG += multimedia-private

SUBDIRS += m3u

qtHaveModule(quick) {
   SUBDIRS += videonode
}

android {
   SUBDIRS += android opensles
}

qnx {
    qtConfig(mmrenderer): SUBDIRS += qnx
    SUBDIRS += audiocapture
}

qnx {
    SUBDIRS += qnx-audio
}

win32 {
    qtConfig(wasapi): SUBDIRS += wasapi
}

win32:!winrt {
    SUBDIRS += audiocapture \
               windowsaudio

    qtConfig(directshow): SUBDIRS += directshow
    qtConfig(wmf): SUBDIRS += wmf
}


winrt {
    SUBDIRS += winrt
}

unix:!mac:!android {
    qtConfig(gstreamer) {
       SUBDIRS += gstreamer
    } else {
        SUBDIRS += audiocapture
    }

    qtConfig(pulseaudio): SUBDIRS += pulseaudio
    qtConfig(alsa): SUBDIRS += alsa

    # v4l is turned off because it is not supported in Qt 5
    # qtConfig(linux_v4l) {
    #     SUBDIRS += v4l
    # }
}

darwin:!watchos {
    SUBDIRS += audiocapture coreaudio
    qtConfig(avfoundation): SUBDIRS += avfoundation
}

qtConfig(resourcepolicy) {
    SUBDIRS += resourcepolicy
}

