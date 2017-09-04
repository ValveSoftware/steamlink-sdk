TARGET = gstaudiodecoder

include(../common.pri)

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/qgstreameraudiodecodercontrol.h \
    $$PWD/qgstreameraudiodecoderservice.h \
    $$PWD/qgstreameraudiodecodersession.h \
    $$PWD/qgstreameraudiodecoderserviceplugin.h

SOURCES += \
    $$PWD/qgstreameraudiodecodercontrol.cpp \
    $$PWD/qgstreameraudiodecoderservice.cpp \
    $$PWD/qgstreameraudiodecodersession.cpp \
    $$PWD/qgstreameraudiodecoderserviceplugin.cpp

OTHER_FILES += \
    audiodecoder.json

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = QGstreamerAudioDecoderServicePlugin
load(qt_plugin)
