TARGET = winrtengine
QT += multimedia-private

LIBS += -lmfplat -lmfuuid -loleaut32 -ld3d11 -lruntimeobject

HEADERS += \
    qwinrtabstractvideorenderercontrol.h \
    qwinrtcameracontrol.h \
    qwinrtcameraflashcontrol.h \
    qwinrtcamerafocuscontrol.h \
    qwinrtcameraimagecapturecontrol.h \
    qwinrtcamerainfocontrol.h \
    qwinrtcameralockscontrol.h \
    qwinrtcameraservice.h \
    qwinrtcameravideorenderercontrol.h \
    qwinrtimageencodercontrol.h \
    qwinrtmediaplayercontrol.h \
    qwinrtmediaplayerservice.h \
    qwinrtplayerrenderercontrol.h \
    qwinrtserviceplugin.h \
    qwinrtvideodeviceselectorcontrol.h \
    qwinrtvideoprobecontrol.h

SOURCES += \
    qwinrtabstractvideorenderercontrol.cpp \
    qwinrtcameracontrol.cpp \
    qwinrtcameraflashcontrol.cpp \
    qwinrtcamerafocuscontrol.cpp \
    qwinrtcameraimagecapturecontrol.cpp \
    qwinrtcamerainfocontrol.cpp \
    qwinrtcameralockscontrol.cpp \
    qwinrtcameraservice.cpp \
    qwinrtcameravideorenderercontrol.cpp \
    qwinrtimageencodercontrol.cpp \
    qwinrtmediaplayercontrol.cpp \
    qwinrtmediaplayerservice.cpp \
    qwinrtplayerrenderercontrol.cpp \
    qwinrtserviceplugin.cpp \
    qwinrtvideodeviceselectorcontrol.cpp \
    qwinrtvideoprobecontrol.cpp

OTHER_FILES += \
    winrt.json

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = WinRTServicePlugin
load(qt_plugin)
