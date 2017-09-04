TARGET = gstmediacapture

include(../common.pri)

INCLUDEPATH += $$PWD

HEADERS += $$PWD/qgstreamercaptureservice.h \
    $$PWD/qgstreamercapturesession.h \
    $$PWD/qgstreameraudioencode.h \
    $$PWD/qgstreamervideoencode.h \
    $$PWD/qgstreamerrecordercontrol.h \
    $$PWD/qgstreamermediacontainercontrol.h \
    $$PWD/qgstreamercameracontrol.h \
    $$PWD/qgstreamercapturemetadatacontrol.h \
    $$PWD/qgstreamerimagecapturecontrol.h \
    $$PWD/qgstreamerimageencode.h \
    $$PWD/qgstreamercaptureserviceplugin.h

SOURCES += $$PWD/qgstreamercaptureservice.cpp \
    $$PWD/qgstreamercapturesession.cpp \
    $$PWD/qgstreameraudioencode.cpp \
    $$PWD/qgstreamervideoencode.cpp \
    $$PWD/qgstreamerrecordercontrol.cpp \
    $$PWD/qgstreamermediacontainercontrol.cpp \
    $$PWD/qgstreamercameracontrol.cpp \
    $$PWD/qgstreamercapturemetadatacontrol.cpp \
    $$PWD/qgstreamerimagecapturecontrol.cpp \
    $$PWD/qgstreamerimageencode.cpp \
    $$PWD/qgstreamercaptureserviceplugin.cpp

# Camera usage with gstreamer needs to have
#CONFIG += use_gstreamer_camera

use_gstreamer_camera:qtConfig(linux_v4l) {
    DEFINES += USE_GSTREAMER_CAMERA

    OTHER_FILES += \
        mediacapturecamera.json

    HEADERS += \
        $$PWD/qgstreamerv4l2input.h
    SOURCES += \
        $$PWD/qgstreamerv4l2input.cpp

} else {
    OTHER_FILES += \
        mediacapture.json
}

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = QGstreamerCaptureServicePlugin
load(qt_plugin)
