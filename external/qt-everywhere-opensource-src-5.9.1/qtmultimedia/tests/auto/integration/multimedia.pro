
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

# QTBUG-60268
boot2qt: SUBDIRS -= qdeclarativevideooutput_window
