INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/bbcameraaudioencodersettingscontrol.h \
    $$PWD/bbcameracapturebufferformatcontrol.h \
    $$PWD/bbcameracapturedestinationcontrol.h \
    $$PWD/bbcameracontrol.h \
    $$PWD/bbcameraexposurecontrol.h \
    $$PWD/bbcameraflashcontrol.h \
    $$PWD/bbcamerafocuscontrol.h \
    $$PWD/bbcameraimagecapturecontrol.h \
    $$PWD/bbcameraimageprocessingcontrol.h \
    $$PWD/bbcamerainfocontrol.h \
    $$PWD/bbcameralockscontrol.h \
    $$PWD/bbcameramediarecordercontrol.h \
    $$PWD/bbcameraorientationhandler.h \
    $$PWD/bbcameraservice.h \
    $$PWD/bbcamerasession.h \
    $$PWD/bbcameravideoencodersettingscontrol.h \
    $$PWD/bbcameraviewfindersettingscontrol.h \
    $$PWD/bbcamerazoomcontrol.h \
    $$PWD/bbimageencodercontrol.h \
    $$PWD/bbmediastoragelocation.h \
    $$PWD/bbvideodeviceselectorcontrol.h \
    $$PWD/bbvideorenderercontrol.h

SOURCES += \
    $$PWD/bbcameraaudioencodersettingscontrol.cpp \
    $$PWD/bbcameracapturebufferformatcontrol.cpp \
    $$PWD/bbcameracapturedestinationcontrol.cpp \
    $$PWD/bbcameracontrol.cpp \
    $$PWD/bbcameraexposurecontrol.cpp \
    $$PWD/bbcameraflashcontrol.cpp \
    $$PWD/bbcamerafocuscontrol.cpp \
    $$PWD/bbcameraimagecapturecontrol.cpp \
    $$PWD/bbcameraimageprocessingcontrol.cpp \
    $$PWD/bbcamerainfocontrol.cpp \
    $$PWD/bbcameralockscontrol.cpp \
    $$PWD/bbcameramediarecordercontrol.cpp \
    $$PWD/bbcameraorientationhandler.cpp \
    $$PWD/bbcameraservice.cpp \
    $$PWD/bbcamerasession.cpp \
    $$PWD/bbcameravideoencodersettingscontrol.cpp \
    $$PWD/bbcameraviewfindersettingscontrol.cpp \
    $$PWD/bbcamerazoomcontrol.cpp \
    $$PWD/bbimageencodercontrol.cpp \
    $$PWD/bbmediastoragelocation.cpp \
    $$PWD/bbvideodeviceselectorcontrol.cpp \
    $$PWD/bbvideorenderercontrol.cpp

LIBS += -lcamapi -laudio_manager

