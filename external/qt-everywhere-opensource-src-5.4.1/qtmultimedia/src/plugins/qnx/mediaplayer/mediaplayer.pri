INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/mmrenderermediaplayercontrol.h \
    $$PWD/mmrenderermediaplayerservice.h \
    $$PWD/mmrenderermetadata.h \
    $$PWD/mmrenderermetadatareadercontrol.h \
    $$PWD/mmrendererplayervideorenderercontrol.h \
    $$PWD/mmrendererutil.h \
    $$PWD/mmrenderervideowindowcontrol.h

SOURCES += \
    $$PWD/mmrenderermediaplayercontrol.cpp \
    $$PWD/mmrenderermediaplayerservice.cpp \
    $$PWD/mmrenderermetadata.cpp \
    $$PWD/mmrenderermetadatareadercontrol.cpp \
    $$PWD/mmrendererplayervideorenderercontrol.cpp \
    $$PWD/mmrendererutil.cpp \
    $$PWD/mmrenderervideowindowcontrol.cpp

LIBS += -lmmrndclient -lstrm

blackberry {
    HEADERS += $$PWD/bpsmediaplayercontrol.h
    SOURCES += $$PWD/bpsmediaplayercontrol.cpp
} else {
    HEADERS += $$PWD/ppsmediaplayercontrol.h
    SOURCES += $$PWD/ppsmediaplayercontrol.cpp
    QT += core-private
    LIBS += -lpps
}
