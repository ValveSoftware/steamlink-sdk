TARGET = ltkutil
include(../../lipilib.pri)

win32: DEFINES -= UNICODE

HEADERS += \
    LTKCheckSumGenerate.h \
    LTKConfigFileReader.h \
    LTKDynamicTimeWarping.h \
    LTKErrors.h \
    LTKHierarchicalClustering.h \
    LTKImageWriter.h \
    LTKInkFileReader.h \
    LTKInkFileWriter.h \
    LTKInkUtils.h \
    LTKLinuxUtil.h \
    LTKLoggerUtil.h \
    LTKStrEncoding.h \
    LTKStringUtil.h \
    LTKVersionCompatibilityCheck.h \
    LTKWinCEUtil.h \
    LTKWindowsUtil.h

SOURCES += \
    LTKCheckSumGenerate.cpp \
    LTKConfigFileReader.cpp \
    LTKInkFileReader.cpp \
    LTKInkFileWriter.cpp \
    LTKLoggerUtil.cpp \
    LTKInkUtils.cpp \
    LTKStrEncoding.cpp \
    LTKErrors.cpp \
    LTKStringUtil.cpp \
    LTKVersionCompatibilityCheck.cpp \
    LTKOSUtilFactory.cpp \
    LTKImageWriter.cpp
    
win32: SOURCES += LTKWindowsUtil.cpp
else: SOURCES += LTKLinuxUtil.cpp
