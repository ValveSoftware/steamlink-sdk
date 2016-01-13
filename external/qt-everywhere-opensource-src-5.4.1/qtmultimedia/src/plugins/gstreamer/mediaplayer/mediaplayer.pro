TARGET = gstmediaplayer

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = QGstreamerPlayerServicePlugin
load(qt_plugin)

include(../common.pri)

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/qgstreamerplayercontrol.h \
    $$PWD/qgstreamerplayerservice.h \
    $$PWD/qgstreamerplayersession.h \
    $$PWD/qgstreamerstreamscontrol.h \
    $$PWD/qgstreamermetadataprovider.h \
    $$PWD/qgstreameravailabilitycontrol.h \
    $$PWD/qgstreamerplayerserviceplugin.h

SOURCES += \
    $$PWD/qgstreamerplayercontrol.cpp \
    $$PWD/qgstreamerplayerservice.cpp \
    $$PWD/qgstreamerplayersession.cpp \
    $$PWD/qgstreamerstreamscontrol.cpp \
    $$PWD/qgstreamermetadataprovider.cpp \
    $$PWD/qgstreameravailabilitycontrol.cpp \
    $$PWD/qgstreamerplayerserviceplugin.cpp

OTHER_FILES += \
    mediaplayer.json

