INCLUDEPATH += $$PWD

LIBS += -lgdi32 -luser32
QMAKE_USE += wmf

HEADERS += \
    $$PWD/mfplayerservice.h \
    $$PWD/mfplayersession.h \
    $$PWD/mfplayercontrol.h \
    $$PWD/mfvideorenderercontrol.h \
    $$PWD/mfaudioendpointcontrol.h \
    $$PWD/mfmetadatacontrol.h \
    $$PWD/mfaudioprobecontrol.h \
    $$PWD/mfvideoprobecontrol.h \
    $$PWD/mfevrvideowindowcontrol.h \
    $$PWD/samplegrabber.h \
    $$PWD/mftvideo.h \
    $$PWD/mfactivate.h

SOURCES += \
    $$PWD/mfplayerservice.cpp \
    $$PWD/mfplayersession.cpp \
    $$PWD/mfplayercontrol.cpp \
    $$PWD/mfvideorenderercontrol.cpp \
    $$PWD/mfaudioendpointcontrol.cpp \
    $$PWD/mfmetadatacontrol.cpp \
    $$PWD/mfaudioprobecontrol.cpp \
    $$PWD/mfvideoprobecontrol.cpp \
    $$PWD/mfevrvideowindowcontrol.cpp \
    $$PWD/samplegrabber.cpp \
    $$PWD/mftvideo.cpp \
    $$PWD/mfactivate.cpp

include($$PWD/../../common/evr.pri)
