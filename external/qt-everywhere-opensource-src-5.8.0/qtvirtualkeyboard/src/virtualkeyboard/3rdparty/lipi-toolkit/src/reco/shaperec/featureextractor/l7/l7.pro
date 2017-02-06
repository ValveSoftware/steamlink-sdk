LIPILIBS = ltkcommon ltkutil featureextractorcommon
include(../../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../../util/lib \
    ../common \

HEADERS += \
    l7.h \
    L7ShapeFeature.h \
    L7ShapeFeatureExtractor.h \

SOURCES += \
    l7.cpp \
    L7ShapeFeature.cpp \
    L7ShapeFeatureExtractor.cpp \

win32 {
    DEFINES += L7_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = l7.def
}
