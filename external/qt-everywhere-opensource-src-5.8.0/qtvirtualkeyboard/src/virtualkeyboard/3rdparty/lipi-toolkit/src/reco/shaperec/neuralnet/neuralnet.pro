LIPILIBS = shaperecommon ltkcommon ltkutil featureextractorcommon
include(../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../util/lib \
    ../featureextractor/common \
    ../common \

HEADERS += \
    NeuralNet.h \
    NeuralNetShapeRecognizer.h \

SOURCES += \
    NeuralNet.cpp \
    NeuralNetShapeRecognizer.cpp \

win32 {
    DEFINES += NEURALNET_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = NeuralNet.def
}
