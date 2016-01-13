TEMPLATE = subdirs

SUBDIRS += \
    audiodecoder \
    mediacapture \
    mediaplayer

config_gstreamer_encodingprofiles {
    SUBDIRS += camerabin
}

OTHER_FILES += \
    gstreamer.json
