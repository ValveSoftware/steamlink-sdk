LIPILIBS = ltkcommon ltkutil featureextractorcommon
include(../../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../../util/lib \
    ../common \

HEADERS += \
    NPen.h \
    NPenShapeFeature.h \
    NPenShapeFeatureExtractor.h \

SOURCES += \
    NPen.cpp \
    NPenShapeFeature.cpp \
    NPenShapeFeatureExtractor.cpp \

win32 {
    DEFINES += NPEN_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = NPen.def
}
