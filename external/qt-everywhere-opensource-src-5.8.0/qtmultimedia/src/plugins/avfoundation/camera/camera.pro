TARGET = qavfcamera

# Avoid clash with a variable named `slots' in a Quartz header
CONFIG += no_keywords

QT += multimedia-private network

LIBS += -framework AudioToolbox \
        -framework CoreAudio \
        -framework QuartzCore \
        -framework CoreMedia
osx:LIBS += -framework AppKit \
            -framework AudioUnit
ios:LIBS += -framework CoreVideo

QMAKE_USE += avfoundation

OTHER_FILES += avfcamera.json

DEFINES += QMEDIA_AVF_CAMERA

HEADERS += \
    avfcameradebug.h \
    avfcameraserviceplugin.h \
    avfcameracontrol.h \
    avfcamerametadatacontrol.h \
    avfimagecapturecontrol.h \
    avfcameraservice.h \
    avfcamerasession.h \
    avfstoragelocation.h \
    avfaudioinputselectorcontrol.h \
    avfcamerainfocontrol.h \
    avfmediavideoprobecontrol.h \
    avfcamerarenderercontrol.h \
    avfcameradevicecontrol.h \
    avfcamerafocuscontrol.h \
    avfcameraexposurecontrol.h \
    avfcamerautility.h \
    avfcameraviewfindersettingscontrol.h \
    avfimageencodercontrol.h \
    avfcameraflashcontrol.h \
    avfvideoencodersettingscontrol.h \
    avfmediacontainercontrol.h \
    avfaudioencodersettingscontrol.h

OBJECTIVE_SOURCES += \
    avfcameraserviceplugin.mm \
    avfcameracontrol.mm \
    avfcamerametadatacontrol.mm \
    avfimagecapturecontrol.mm \
    avfcameraservice.mm \
    avfcamerasession.mm \
    avfstoragelocation.mm \
    avfaudioinputselectorcontrol.mm \
    avfcamerainfocontrol.mm \
    avfmediavideoprobecontrol.mm \
    avfcameradevicecontrol.mm \
    avfcamerarenderercontrol.mm \
    avfcamerafocuscontrol.mm \
    avfcameraexposurecontrol.mm \
    avfcamerautility.mm \
    avfcameraviewfindersettingscontrol.mm \
    avfimageencodercontrol.mm \
    avfcameraflashcontrol.mm \
    avfvideoencodersettingscontrol.mm \
    avfmediacontainercontrol.mm \
    avfaudioencodersettingscontrol.mm

osx {

HEADERS += avfmediarecordercontrol.h
OBJECTIVE_SOURCES += avfmediarecordercontrol.mm

}

ios {

HEADERS += avfcamerazoomcontrol.h \
           avfmediaassetwriter.h \
           avfmediarecordercontrol_ios.h
OBJECTIVE_SOURCES += avfcamerazoomcontrol.mm \
                     avfmediaassetwriter.mm \
                     avfmediarecordercontrol_ios.mm

}

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = AVFServicePlugin
load(qt_plugin)
