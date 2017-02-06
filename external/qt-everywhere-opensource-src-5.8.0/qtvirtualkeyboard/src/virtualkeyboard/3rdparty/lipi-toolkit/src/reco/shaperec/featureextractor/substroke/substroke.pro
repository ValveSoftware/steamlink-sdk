LIPILIBS = ltkcommon ltkutil featureextractorcommon
include(../../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../../util/lib \
    ../common \

HEADERS += \
    SubStroke.h \
    SubStrokeShapeFeature.h \
    SubStrokeShapeFeatureExtractor.h \

SOURCES += \
    SubStroke.cpp \
    SubStrokeShapeFeature.cpp \
    SubStrokeShapeFeatureExtractor.cpp \

win32 {
    DEFINES += SUBSTROKE_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = SubStroke.def
}
