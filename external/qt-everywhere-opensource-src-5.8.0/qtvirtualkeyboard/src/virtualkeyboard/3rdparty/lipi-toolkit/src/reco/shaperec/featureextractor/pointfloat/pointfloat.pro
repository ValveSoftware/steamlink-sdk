LIPILIBS = ltkcommon ltkutil featureextractorcommon
include(../../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../../util/lib \
    ../common \

HEADERS += \
    PointFloat.h \
    PointFloatShapeFeature.h \
    PointFloatShapeFeatureExtractor.h \

SOURCES += \
    PointFloat.cpp \
    PointFloatShapeFeature.cpp \
    PointFloatShapeFeatureExtractor.cpp \

win32 {
    DEFINES += POINTFLOAT_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = PointFloat.def
}
