TARGET = qtaudio_coreaudio
QT += multimedia-private

OTHER_FILES += \
    coreaudio.json

#DEFINES += QT_DEBUG_COREAUDIO

HEADERS += \
    coreaudiodeviceinfo.h \
    coreaudioinput.h \
    coreaudiooutput.h \
    coreaudioplugin.h \
    coreaudioutils.h

OBJECTIVE_SOURCES += \
    coreaudiodeviceinfo.mm \
    coreaudioinput.mm \
    coreaudiooutput.mm \
    coreaudioplugin.mm \
    coreaudioutils.mm

ios|tvos {
    HEADERS += coreaudiosessionmanager.h
    OBJECTIVE_SOURCES += coreaudiosessionmanager.mm
    LIBS += -framework Foundation -framework AVFoundation
} else {
    LIBS += \
        -framework ApplicationServices \
        -framework AudioUnit
}

LIBS += \
    -framework CoreAudio \
    -framework AudioToolbox

PLUGIN_TYPE = audio
PLUGIN_CLASS_NAME = CoreAudioPlugin
load(qt_plugin)
