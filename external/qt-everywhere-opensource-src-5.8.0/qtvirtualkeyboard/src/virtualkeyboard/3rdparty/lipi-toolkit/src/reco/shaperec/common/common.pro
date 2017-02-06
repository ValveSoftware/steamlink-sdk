TARGET = shaperecommon
include(../../../lipilib.pri)

INCLUDEPATH += \
    ../../../util/lib \
    ../featureextractor/common \

SOURCES += \
    LTKShapeRecoConfig.cpp \
    LTKShapeRecognizer.cpp \
    LTKShapeRecoResult.cpp \
    LTKShapeRecoUtil.cpp \
    LTKShapeSample.cpp
