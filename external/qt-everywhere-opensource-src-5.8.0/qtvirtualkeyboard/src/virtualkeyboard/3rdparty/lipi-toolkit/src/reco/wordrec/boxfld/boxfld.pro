LIPILIBS = shaperecommon ltkcommon ltkutil wordreccommon
include(../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../util/lib \
    ../common \

HEADERS += \
    BoxFieldRecognizer.h \
    boxfld.h \

SOURCES += \
    BoxFieldRecognizer.cpp \
    boxfld.cpp \

win32 {
    DEFINES += BOXFLD_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = boxfld.def
}
