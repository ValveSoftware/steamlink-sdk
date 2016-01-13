INCLUDEPATH += audio

PUBLIC_HEADERS += \
           audio/qaudio.h \
           audio/qaudiobuffer.h \
           audio/qaudioformat.h \
           audio/qaudioinput.h \
           audio/qaudiooutput.h \
           audio/qaudiodeviceinfo.h \
           audio/qaudiosystemplugin.h \
           audio/qaudiosystem.h  \
           audio/qsoundeffect.h \
           audio/qsound.h \
           audio/qaudioprobe.h \
           audio/qaudiodecoder.h

PRIVATE_HEADERS += \
           audio/qaudiobuffer_p.h \
           audio/qaudiodevicefactory_p.h \
           audio/qwavedecoder_p.h \
           audio/qsamplecache_p.h \
           audio/qaudiohelpers_p.h

SOURCES += \
           audio/qaudio.cpp \
           audio/qaudioformat.cpp  \
           audio/qaudiodeviceinfo.cpp \
           audio/qaudiooutput.cpp \
           audio/qaudioinput.cpp \
           audio/qaudiosystemplugin.cpp \
           audio/qaudiosystem.cpp \
           audio/qaudiodevicefactory.cpp \
           audio/qsoundeffect.cpp \
           audio/qwavedecoder_p.cpp \
           audio/qsamplecache_p.cpp \
           audio/qsound.cpp \
           audio/qaudiobuffer.cpp \
           audio/qaudioprobe.cpp \
           audio/qaudiodecoder.cpp \
           audio/qaudiohelpers.cpp

unix:!mac {
    config_pulseaudio {
        CONFIG += link_pkgconfig
        PKGCONFIG_PRIVATE += libpulse

        DEFINES += QT_MULTIMEDIA_PULSEAUDIO
        PRIVATE_HEADERS += audio/qsoundeffect_pulse_p.h
        SOURCES += audio/qsoundeffect_pulse_p.cpp
        !maemo*:DEFINES += QTM_PULSEAUDIO_DEFAULTBUFFER
    } else {
        DEFINES += QT_MULTIMEDIA_QAUDIO
        PRIVATE_HEADERS += audio/qsoundeffect_qaudio_p.h
        SOURCES += audio/qsoundeffect_qaudio_p.cpp
    }
} else {
    DEFINES += QT_MULTIMEDIA_QAUDIO
    PRIVATE_HEADERS += audio/qsoundeffect_qaudio_p.h
    SOURCES += audio/qsoundeffect_qaudio_p.cpp
}
