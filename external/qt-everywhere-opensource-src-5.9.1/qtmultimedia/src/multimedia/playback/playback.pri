INCLUDEPATH += playback

PUBLIC_HEADERS += \
    playback/qmediacontent.h \
    playback/qmediaplayer.h \
    playback/qmediaplaylist.h \
    playback/qmediaresource.h

PRIVATE_HEADERS += \
    playback/qmediaplaylist_p.h \
    playback/qmediaplaylistprovider_p.h \
    playback/qmediaplaylistioplugin_p.h \
    playback/qmediaplaylistnavigator_p.h \
    playback/qmedianetworkplaylistprovider_p.h \
    playback/qplaylistfileparser_p.h

SOURCES += \
    playback/qmedianetworkplaylistprovider.cpp \
    playback/qmediacontent.cpp \
    playback/qmediaplayer.cpp \
    playback/qmediaplaylist.cpp \
    playback/qmediaplaylistioplugin.cpp \
    playback/qmediaplaylistnavigator.cpp \
    playback/qmediaplaylistprovider.cpp \
    playback/qmediaresource.cpp \
    playback/qplaylistfileparser.cpp
