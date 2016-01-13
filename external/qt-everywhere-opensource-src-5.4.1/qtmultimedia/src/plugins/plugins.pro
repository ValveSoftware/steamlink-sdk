######################################################################
#
# Qt Multimedia
#
######################################################################

TEMPLATE = subdirs

SUBDIRS += m3u videonode

android {
   SUBDIRS += android opensles
}

qnx {
    config_mmrenderer: SUBDIRS += qnx
    SUBDIRS += audiocapture
}

qnx:!blackberry {
    SUBDIRS += qnx-audio
}

win32:!winrt {
    SUBDIRS += audiocapture \
               windowsaudio

    config_directshow: SUBDIRS += directshow
    config_wmf: SUBDIRS += wmf
}

winrt {
    SUBDIRS += winrt
}

unix:!mac:!android {
    config_gstreamer {
       SUBDIRS += gstreamer
    } else {
        SUBDIRS += audiocapture
    }

    config_pulseaudio: SUBDIRS += pulseaudio
    config_alsa: SUBDIRS += alsa

    # v4l is turned off because it is not supported in Qt 5
    # config_linux_v4l {
    #     !maemo*:SUBDIRS += v4l
    # }
}

mac:!simulator {
    SUBDIRS += audiocapture coreaudio

    config_avfoundation: SUBDIRS += avfoundation
}

config_resourcepolicy {
    SUBDIRS += resourcepolicy
}

