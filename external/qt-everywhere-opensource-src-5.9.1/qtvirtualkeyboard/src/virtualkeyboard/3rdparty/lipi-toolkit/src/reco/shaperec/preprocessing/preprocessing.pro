TARGET = preproc
LIPILIBS = shaperecommon ltkcommon ltkutil
include(../../../lipiplugin.pri)

INCLUDEPATH += \
    ../../../util/lib \
    ../common \

HEADERS += \
    preprocessing.h \

SOURCES += \
    LTKPreprocessor.cpp \
    preprocessing.cpp \

win32 {
    DEFINES += PREPROCESSING_EXPORTS
    LIBS += Advapi32.lib
    #DEF_FILE = preprocessing.def
}
