TARGET = qtaudio_wasapi
QT += core-private multimedia-private

HEADERS += \
    qwasapiplugin.h \
    qwasapiaudiodeviceinfo.h \
    qwasapiaudioinput.h \
    qwasapiaudiooutput.h \
    qwasapiutils.h

SOURCES += \
    qwasapiplugin.cpp \
    qwasapiaudiodeviceinfo.cpp \
    qwasapiaudioinput.cpp \
    qwasapiaudiooutput.cpp \
    qwasapiutils.cpp

OTHER_FILES += \
    wasapi.json

LIBS += Mmdevapi.lib

win32-* {
    DEFINES += CLASSIC_APP_BUILD
    LIBS += runtimeobject.lib
}

PLUGIN_TYPE = audio
PLUGIN_CLASS_NAME = QWasapiPlugin
load(qt_plugin)
