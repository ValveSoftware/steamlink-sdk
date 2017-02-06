# Camera related mock backend files
INCLUDEPATH += $$PWD \
    ../../../../src/multimedia \
    ../../../../src/multimedia/video \
    ../../../../src/multimedia/camera

HEADERS *= \
    ../qmultimedia_common/mockcameraservice.h \
    ../qmultimedia_common/mockcameraflashcontrol.h \
    ../qmultimedia_common/mockcameralockscontrol.h \
    ../qmultimedia_common/mockcamerafocuscontrol.h \
    ../qmultimedia_common/mockcamerazoomcontrol.h \
    ../qmultimedia_common/mockcameraimageprocessingcontrol.h \
    ../qmultimedia_common/mockcameraimagecapturecontrol.h \
    ../qmultimedia_common/mockcameraexposurecontrol.h \
    ../qmultimedia_common/mockcameracapturedestinationcontrol.h \
    ../qmultimedia_common/mockcameracapturebuffercontrol.h \
    ../qmultimedia_common/mockimageencodercontrol.h \
    ../qmultimedia_common/mockcameracontrol.h \
    ../qmultimedia_common/mockvideodeviceselectorcontrol.h \
    ../qmultimedia_common/mockcamerainfocontrol.h \
    ../qmultimedia_common/mockcameraviewfindersettingscontrol.h


include(mockvideo.pri)

