TARGET = wmfengine
QT += multimedia-private network

win32:!qtHaveModule(opengl) {
    LIBS_PRIVATE += -lgdi32 -luser32
}

INCLUDEPATH += .

HEADERS += \
    wmfserviceplugin.h \
    mfstream.h \
    sourceresolver.h

SOURCES += \
    wmfserviceplugin.cpp \
    mfstream.cpp \
    sourceresolver.cpp

qtConfig(wmf-player): include (player/player.pri)
include (decoder/decoder.pri)

OTHER_FILES += \
    wmf.json \
    wmf_audiodecode.json

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = WMFServicePlugin
load(qt_plugin)
