TARGET = qtmedia_android
QT += multimedia-private core-private network

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = QAndroidMediaServicePlugin
load(qt_plugin)

HEADERS += \
    qandroidmediaserviceplugin.h

SOURCES += \
    qandroidmediaserviceplugin.cpp

include (wrappers/jni/jni.pri)
include (common/common.pri)
include (mediaplayer/mediaplayer.pri)
include (mediacapture/mediacapture.pri)

OTHER_FILES += android_mediaservice.json
