TARGET = qtmedia_qnx
QT += multimedia-private gui-private

PLUGIN_TYPE=mediaservice
PLUGIN_CLASS_NAME = BbServicePlugin
load(qt_plugin)

LIBS += -lscreen

include(common/common.pri)
include(mediaplayer/mediaplayer.pri)

blackberry {
    include(camera/camera.pri)
    HEADERS += bbserviceplugin.h
    SOURCES += bbserviceplugin.cpp
    OTHER_FILES += blackberry_mediaservice.json
} else {
    HEADERS += neutrinoserviceplugin.h
    SOURCES += neutrinoserviceplugin.cpp
    OTHER_FILES += neutrino_mediaservice.json
}
