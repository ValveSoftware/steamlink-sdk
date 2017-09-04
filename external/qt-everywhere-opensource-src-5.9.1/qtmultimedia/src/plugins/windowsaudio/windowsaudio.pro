TARGET = qtaudio_windows
QT += multimedia-private

LIBS += -lstrmiids -lole32 -loleaut32 -lwinmm

HEADERS += \
    qwindowsaudioplugin.h \
    qwindowsaudiodeviceinfo.h \
    qwindowsaudioinput.h \
    qwindowsaudiooutput.h \
    qwindowsaudioutils.h

SOURCES += \
    qwindowsaudioplugin.cpp \
    qwindowsaudiodeviceinfo.cpp \
    qwindowsaudioinput.cpp \
    qwindowsaudiooutput.cpp \
    qwindowsaudioutils.cpp

OTHER_FILES += \
    windowsaudio.json

PLUGIN_TYPE = audio
PLUGIN_CLASS_NAME = QWindowsAudioPlugin
load(qt_plugin)
