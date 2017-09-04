LIPILIBS = shaperecommon ltkcommon ltkutil featureextractorcommon
include(../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../util/lib \
    ../featureextractor/common \
    ../common \

HEADERS += \
    NN.h \
    NNAdapt.h \
    NNShapeRecognizer.h \

SOURCES += \
    NN.cpp \
    NNAdapt.cpp \
    NNShapeRecognizer.cpp \

win32 {
    DEFINES += NN_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = NN.def
}
