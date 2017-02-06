LIPILIBS = shaperecommon ltkcommon ltkutil wordreccommon
include(../lipiplugin.pri)

INCLUDEPATH += \
    ../util/lib

HEADERS += \
    lipiengine.h \
    LipiEngineModule.h

SOURCES += \
    lipiengine.cpp \
    LipiEngineModule.cpp

win32 {
    DEFINES += LIPIENGINE_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = lipiengine.def
}
