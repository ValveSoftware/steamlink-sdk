TARGET = ltkcommon
include(../lipilib.pri)

INCLUDEPATH += \
    ../util/lib

SOURCES += \
    LTKCaptureDevice.cpp \
    LTKChannel.cpp \
    LTKException.cpp \
    LTKScreenContext.cpp \
    LTKTrace.cpp \
    LTKTraceFormat.cpp \
    LTKTraceGroup.cpp

include(../include/headers.pri)
