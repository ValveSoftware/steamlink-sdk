TARGET = qtmedia_audioengine
QT += multimedia-private

HEADERS += audioencodercontrol.h \
    audiocontainercontrol.h \
    audiomediarecordercontrol.h \
    audioinputselector.h \
    audiocaptureservice.h \
    audiocaptureserviceplugin.h \
    audiocapturesession.h \
    audiocaptureprobecontrol.h

SOURCES += audioencodercontrol.cpp \
    audiocontainercontrol.cpp \
    audiomediarecordercontrol.cpp \
    audioinputselector.cpp \
    audiocaptureservice.cpp \
    audiocaptureserviceplugin.cpp \
    audiocapturesession.cpp \
    audiocaptureprobecontrol.cpp

OTHER_FILES += \
    audiocapture.json

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = AudioCaptureServicePlugin
load(qt_plugin)
