LIPILIBS = shaperecommon ltkcommon ltkutil featureextractorcommon
include(../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../util/lib \
    ../featureextractor/common \
    ../common \

HEADERS += \
    ActiveDTW.h \
    ActiveDTWAdapt.h \
    ActiveDTWClusterModel.h \
    ActiveDTWShapeModel.h \
    ActiveDTWShapeRecognizer.h \

SOURCES += \
    ActiveDTW.cpp \
    ActiveDTWShapeRecognizer.cpp\
    ActiveDTWClusterModel.cpp \
    ActiveDTWShapeModel.cpp \
    ActiveDTWAdapt.cpp \

win32 {
    DEFINES += ACTIVEDTW_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = ActiveDTW.def
}
