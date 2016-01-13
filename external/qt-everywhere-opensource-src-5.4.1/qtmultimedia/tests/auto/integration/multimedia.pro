
TEMPLATE = subdirs
SUBDIRS += \
    qaudiodecoderbackend \
    qaudiodeviceinfo \
    qaudioinput \
    qaudiooutput \
    qmediaplayerbackend \
    qcamerabackend \
    qsoundeffect \
    qsound

qtHaveModule(quick) {
    SUBDIRS += \
        qdeclarativevideooutput \
        qdeclarativevideooutput_window
}

!qtHaveModule(widgets): SUBDIRS -= qcamerabackend
